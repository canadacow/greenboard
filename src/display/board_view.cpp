#include "display/board_view.h"
#include "core/signal.h"

#include <d3dcompiler.h>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <cmath>
#include <algorithm>

using Microsoft::WRL::ComPtr;
using json = nlohmann::json;

namespace bench {

// ========================================================================
// HLSL shaders (compiled at init via D3DCompile)
// ========================================================================

static const char* k_shader_source = R"HLSL(

// Structured buffer of all board segments.
struct Segment {
    float x1, y1, x2, y2;
    float half_w;
    uint  rgba;
    uint  net;
    uint  flags;  // 0=top, 1=bot, 2=via, 3=pad, 4=outline
};

StructuredBuffer<Segment> segs : register(t0);
Buffer<uint> net_levels : register(t1);  // per-net signal level (0=Low,1=HiZ,2=High,0xFF=unbound)

cbuffer View : register(b0) {
    float2 view_min;
    float2 view_size;
    float2 screen_size;
    uint   seg_count;
    uint   highlight_net;
    float4 bg_color;
    uint   layer_mask;
    float3 _pad;
};

struct VS_OUT {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

// Fullscreen triangle from vertex ID (no vertex buffer).
VS_OUT vs_main(uint id : SV_VertexID) {
    VS_OUT o;
    o.uv  = float2((id << 1) & 2, id & 2);
    o.pos = float4(o.uv * 2.0 - 1.0, 0.0, 1.0);
    o.pos.y = -o.pos.y;
    return o;
}

float4 unpack_rgba(uint c) {
    return float4(
        float(c & 0xFF) / 255.0,
        float((c >> 8) & 0xFF) / 255.0,
        float((c >> 16) & 0xFF) / 255.0,
        float((c >> 24) & 0xFF) / 255.0
    );
}

// SDF: distance from point p to line segment a-b.
float sd_segment(float2 p, float2 a, float2 b) {
    float2 pa = p - a;
    float2 ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-8), 0.0, 1.0);
    return length(pa - ba * h);
}

// Layer visibility check via bitmask.
bool layer_visible(uint flags) {
    // flags: 0=top, 1=bot, 2=via, 3=pad, 4=outline
    return (layer_mask & (1u << min(flags, 4u))) != 0;
}

float4 ps_main(VS_OUT input) : SV_Target {
    float2 p = view_min + input.uv * view_size;

    // Pixel size in board-space (for anti-aliasing).
    float px = view_size.x / screen_size.x;

    float min_dist = 1e9;
    uint  closest  = 0;

    for (uint i = 0; i < seg_count; i++) {
        if (!layer_visible(segs[i].flags)) continue;

        // AABB early-out: expand segment bbox by half_w + 2px AA margin.
        float margin = segs[i].half_w + px * 2.0;
        float2 mn = min(float2(segs[i].x1, segs[i].y1),
                        float2(segs[i].x2, segs[i].y2)) - margin;
        float2 mx = max(float2(segs[i].x1, segs[i].y1),
                        float2(segs[i].x2, segs[i].y2)) + margin;
        if (p.x < mn.x || p.x > mx.x || p.y < mn.y || p.y > mx.y)
            continue;

        float d = sd_segment(p, float2(segs[i].x1, segs[i].y1),
                                float2(segs[i].x2, segs[i].y2));
        d -= segs[i].half_w;

        if (d < min_dist) {
            min_dist = d;
            closest  = i;
        }
    }

    // Background: dark PCB green.
    float4 bg = bg_color;

    if (min_dist > px * 2.0)
        return bg;

    float4 seg_color = unpack_rgba(segs[closest].rgba);

    // Modulate color by live signal level.
    uint lvl = net_levels[segs[closest].net];
    if (lvl == 2u)       seg_color.rgb *= 2.0;                        // High: bright
    else if (lvl == 0u)  seg_color.rgb *= 0.25;                       // Low: dim
    else if (lvl == 1u)  seg_color.rgb = float3(0.25, 0.25, 0.25);   // HiZ: gray
    // else 0xFF (unbound): use base color as-is.

    // Highlight: brighten if this net is selected.
    if (highlight_net != 0 && segs[closest].net == highlight_net)
        seg_color = lerp(seg_color, float4(1, 1, 1, 1), 0.5);

    // Anti-alias: smooth transition over 1.5 pixels.
    float aa = smoothstep(-px * 0.75, px * 0.75, min_dist);
    return lerp(seg_color, bg, aa);
}
)HLSL";

// ========================================================================
// Color palette
// ========================================================================

static uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 220) {
    return r | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24);
}

static const uint32_t COL_TOP     = rgba(200, 60, 60);       // red
static const uint32_t COL_BOT     = rgba(60, 90, 200);       // blue
static const uint32_t COL_VIA     = rgba(60, 200, 180, 255); // cyan
static const uint32_t COL_PAD     = rgba(200, 200, 60, 255); // yellow
static const uint32_t COL_OUTLINE = rgba(140, 140, 140, 160);// gray
static const uint32_t COL_BOARD   = rgba(220, 220, 220, 255);// light gray

// ========================================================================
// JSON loading
// ========================================================================

bool BoardView::load_json(const char* path) {
    std::ifstream f(path);
    if (!f) {
        spdlog::error("[BoardView] Cannot open {}", path);
        return false;
    }

    json j;
    try {
        j = json::parse(f);
    } catch (const json::exception& e) {
        spdlog::error("[BoardView] JSON parse error: {}", e.what());
        return false;
    }

    // Bounds
    auto& b = j["bounds"];
    bounds_[0] = b["x_min"].get<float>();
    bounds_[1] = b["y_min"].get<float>();
    bounds_[2] = b["x_max"].get<float>();
    bounds_[3] = b["y_max"].get<float>();

    // Net names
    for (auto& [k, v] : j["net_names"].items())
        net_names_[static_cast<uint32_t>(std::stoul(k))] = v.get<std::string>();

    // Traces
    for (auto& t : j["traces"]) {
        GpuSegment s;
        s.x1 = t["x1"].get<float>();
        s.y1 = t["y1"].get<float>();
        s.x2 = t["x2"].get<float>();
        s.y2 = t["y2"].get<float>();
        s.half_w = t["w"].get<float>() * 0.5f;
        s.net = t["net"].get<uint32_t>();
        int layer = t["layer"].get<int>();
        s.flags = (layer == 15) ? 0 : 1;  // 0=top, 1=bottom
        s.rgba = (layer == 15) ? COL_TOP : COL_BOT;
        segments_.push_back(s);
    }

    // Vias
    for (auto& v : j["vias"]) {
        GpuSegment s;
        s.x1 = s.x2 = v["x"].get<float>();
        s.y1 = s.y2 = v["y"].get<float>();
        s.half_w = v["dia"].get<float>() * 0.5f;
        s.net = v["net"].get<uint32_t>();
        s.flags = 2;
        s.rgba = COL_VIA;
        segments_.push_back(s);
    }

    // Components: transform pads + outlines from local to world coords.
    for (auto& c : j["components"]) {
        float mx = c["x"].get<float>();
        float my = c["y"].get<float>();
        float orient = c["orient"].get<float>();
        float rad = orient * 3.14159265f / 1800.0f;
        float cos_a = std::cos(rad);
        float sin_a = std::sin(rad);

        auto xform = [&](float lx, float ly, float& wx, float& wy) {
            wx = mx + lx * cos_a - ly * sin_a;
            wy = my + lx * sin_a + ly * cos_a;
        };

        // Pads (signal only)
        for (auto& p : c["pads"]) {
            uint32_t pnet = p["net"].get<uint32_t>();
            if (pnet == 0 || pnet == 1 || pnet == 2 || pnet == 3 || pnet == 4 || pnet == 84)
                continue;  // skip power

            float lx = p["x"].get<float>();
            float ly = p["y"].get<float>();
            float pw = p["w"].get<float>();
            float ph = p["h"].get<float>();

            GpuSegment s;
            xform(lx, ly, s.x1, s.y1);
            s.x2 = s.x1;
            s.y2 = s.y1;
            s.half_w = (std::min)(pw, ph) * 0.5f;
            s.net = pnet;
            s.flags = 3;
            s.rgba = COL_PAD;
            segments_.push_back(s);
        }

        // Outlines
        for (auto& o : c["outline"]) {
            float lx1 = o["x1"].get<float>(), ly1 = o["y1"].get<float>();
            float lx2 = o["x2"].get<float>(), ly2 = o["y2"].get<float>();

            GpuSegment s;
            xform(lx1, ly1, s.x1, s.y1);
            xform(lx2, ly2, s.x2, s.y2);
            s.half_w = 1.5f;  // thin outline
            s.net = 0;
            s.flags = 4;
            s.rgba = COL_OUTLINE;
            segments_.push_back(s);
        }

        // Label
        CompLabel lbl;
        lbl.ref = c["ref"].get<std::string>();
        lbl.value = c.value("value", "");
        lbl.x = mx;
        lbl.y = my;
        labels_.push_back(std::move(lbl));
    }

    // Board outline
    for (auto& o : j["board_outline"]) {
        GpuSegment s;
        s.x1 = o["x1"].get<float>();
        s.y1 = o["y1"].get<float>();
        s.x2 = o["x2"].get<float>();
        s.y2 = o["y2"].get<float>();
        s.half_w = 5.0f;
        s.net = 0;
        s.flags = 4;
        s.rgba = COL_BOARD;
        segments_.push_back(s);
    }

    // Find max net ID for sizing the level buffer.
    for (auto& [nid, _] : net_names_)
        if (nid > max_net_id_) max_net_id_ = nid;

    spdlog::info("[BoardView] Loaded {} segments, {} nets (max ID {}), {} components",
                 segments_.size(), net_names_.size(), max_net_id_, labels_.size());
    return true;
}

// ========================================================================
// GPU resource creation
// ========================================================================

static ComPtr<ID3DBlob> compile_shader(const char* src, const char* entry,
                                        const char* target) {
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompile(src, strlen(src), nullptr, nullptr, nullptr,
                            entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                            &blob, &err);
    if (FAILED(hr)) {
        if (err)
            spdlog::error("[BoardView] Shader compile: {}",
                          (const char*)err->GetBufferPointer());
        return nullptr;
    }
    return blob;
}

bool BoardView::create_gpu_resources(ID3D11Device* device) {
    // Compile shaders
    auto vs_blob = compile_shader(k_shader_source, "vs_main", "vs_5_0");
    auto ps_blob = compile_shader(k_shader_source, "ps_main", "ps_5_0");
    if (!vs_blob || !ps_blob) return false;

    device->CreateVertexShader(vs_blob->GetBufferPointer(),
                               vs_blob->GetBufferSize(), nullptr, &vs_);
    device->CreatePixelShader(ps_blob->GetBufferPointer(),
                              ps_blob->GetBufferSize(), nullptr, &ps_);

    // Constant buffer
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = sizeof(ViewCB);
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&cbd, nullptr, &cb_);

    // Structured buffer for segments
    D3D11_BUFFER_DESC sbd = {};
    sbd.ByteWidth = static_cast<UINT>(segments_.size() * sizeof(GpuSegment));
    sbd.Usage = D3D11_USAGE_IMMUTABLE;
    sbd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    sbd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    sbd.StructureByteStride = sizeof(GpuSegment);

    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = segments_.data();
    device->CreateBuffer(&sbd, &srd, &seg_buf_);

    // SRV for structured buffer
    D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
    svd.Format = DXGI_FORMAT_UNKNOWN;
    svd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    svd.Buffer.NumElements = static_cast<UINT>(segments_.size());
    device->CreateShaderResourceView(seg_buf_.Get(), &svd, &seg_srv_);

    // Per-net signal level buffer (dynamic, updated each frame).
    // Indexed by BRD net ID. R32_UINT typed buffer.
    uint32_t lvl_count = max_net_id_ + 1;
    net_levels_.resize(lvl_count, 0xFFu);  // 0xFF = unbound

    D3D11_BUFFER_DESC lbd = {};
    lbd.ByteWidth = lvl_count * sizeof(uint32_t);
    lbd.Usage = D3D11_USAGE_DYNAMIC;
    lbd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    lbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA lrd = {};
    lrd.pSysMem = net_levels_.data();
    device->CreateBuffer(&lbd, &lrd, &lvl_buf_);

    D3D11_SHADER_RESOURCE_VIEW_DESC lvd = {};
    lvd.Format = DXGI_FORMAT_R32_UINT;
    lvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    lvd.Buffer.NumElements = lvl_count;
    device->CreateShaderResourceView(lvl_buf_.Get(), &lvd, &lvl_srv_);

    return true;
}

// ========================================================================
// Render target management
// ========================================================================

void BoardView::ensure_rt(ID3D11DeviceContext* ctx, int w, int h) {
    if (w == rt_w_ && h == rt_h_ && rt_tex_) return;
    if (w <= 0 || h <= 0) return;

    rt_tex_.Reset();
    rt_rtv_.Reset();
    rt_srv_.Reset();

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    device_->CreateTexture2D(&td, nullptr, &rt_tex_);
    device_->CreateRenderTargetView(rt_tex_.Get(), nullptr, &rt_rtv_);
    device_->CreateShaderResourceView(rt_tex_.Get(), nullptr, &rt_srv_);

    rt_w_ = w;
    rt_h_ = h;
}

// ========================================================================
// SDF render pass
// ========================================================================

void BoardView::render_to_texture(ID3D11DeviceContext* ctx, int w, int h) {
    ensure_rt(ctx, w, h);
    if (!rt_rtv_) return;

    // Update constant buffer
    float board_w = (bounds_[2] - bounds_[0]);
    float board_h = (bounds_[3] - bounds_[1]);
    float view_w = (float)w / zoom_;
    float view_h = (float)h / zoom_;

    ViewCB cb;
    cb.view_min_x = pan_x_ - view_w * 0.5f;
    cb.view_min_y = pan_y_ - view_h * 0.5f;
    cb.view_size_x = view_w;
    cb.view_size_y = view_h;
    cb.screen_w = (float)w;
    cb.screen_h = (float)h;
    cb.seg_count = static_cast<uint32_t>(segments_.size());
    cb.highlight_net = highlight_net_;
    // Dark PCB green
    cb.bg_r = 0.02f; cb.bg_g = 0.08f; cb.bg_b = 0.02f; cb.bg_a = 1.0f;
    cb.layer_mask = layer_mask_;
    cb.pad[0] = cb.pad[1] = cb.pad[2] = 0;

    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(cb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, &cb, sizeof(cb));
    ctx->Unmap(cb_.Get(), 0);

    // Save current state
    ComPtr<ID3D11RenderTargetView> old_rtv;
    ComPtr<ID3D11DepthStencilView> old_dsv;
    D3D11_VIEWPORT old_vp;
    UINT num_vp = 1;
    ctx->RSGetViewports(&num_vp, &old_vp);
    ctx->OMGetRenderTargets(1, &old_rtv, &old_dsv);

    // Bind our render target
    ctx->OMSetRenderTargets(1, rt_rtv_.GetAddressOf(), nullptr);
    D3D11_VIEWPORT vp = { 0, 0, (float)w, (float)h, 0, 1 };
    ctx->RSSetViewports(1, &vp);

    // Set shaders and resources
    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, cb_.GetAddressOf());
    ID3D11ShaderResourceView* srvs[2] = { seg_srv_.Get(), lvl_srv_.Get() };
    ctx->PSSetShaderResources(0, 2, srvs);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(nullptr);

    // Draw fullscreen triangle
    ctx->Draw(3, 0);

    // Unbind SRV (so texture can be used as ImGui image)
    ID3D11ShaderResourceView* null_srvs[2] = { nullptr, nullptr };
    ctx->PSSetShaderResources(0, 2, null_srvs);

    // Restore state
    ctx->OMSetRenderTargets(1, old_rtv.GetAddressOf(), old_dsv.Get());
    ctx->RSSetViewports(1, &old_vp);
}

// ========================================================================
// CPU-side hit test (for tooltip/highlight)
// ========================================================================

uint32_t BoardView::hit_test(float bx, float by) const {
    float best_dist = 1e9f;
    uint32_t best_net = 0;

    for (auto& s : segments_) {
        if (s.net == 0) continue;

        // SDF: distance to rounded line segment
        float ax = s.x1, ay = s.y1, bxx = s.x2, byy = s.y2;
        float pax = bx - ax, pay = by - ay;
        float bax = bxx - ax, bay = byy - ay;
        float denom = bax * bax + bay * bay;
        float h = (denom > 1e-8f) ?
            std::clamp((pax * bax + pay * bay) / denom, 0.0f, 1.0f) : 0.0f;
        float dx = pax - bax * h;
        float dy = pay - bay * h;
        float d = std::sqrt(dx * dx + dy * dy) - s.half_w;

        if (d < best_dist) {
            best_dist = d;
            best_net = s.net;
        }
    }

    return (best_dist <= 0.0f) ? best_net : 0;
}

// ========================================================================
// Init
// ========================================================================

bool BoardView::init(ID3D11Device* device, const char* json_path) {
    device_ = device;

    if (!load_json(json_path)) return false;
    if (segments_.empty()) {
        spdlog::error("[BoardView] No segments loaded");
        return false;
    }
    if (!create_gpu_resources(device)) return false;

    // Center view on board
    pan_x_ = (bounds_[0] + bounds_[2]) * 0.5f;
    pan_y_ = (bounds_[1] + bounds_[3]) * 0.5f;
    // Default zoom: fit board width into ~800px
    zoom_ = 800.0f / (bounds_[2] - bounds_[0]);

    return true;
}

// ========================================================================
// Signal binding
// ========================================================================

void BoardView::bind_signals(const std::unordered_map<std::string, int>& brd_map) {
    net_to_pool_.clear();
    int bound = 0;
    for (auto& [net_id, net_name] : net_names_) {
        auto it = brd_map.find(net_name);
        if (it != brd_map.end()) {
            net_to_pool_[net_id] = it->second;
            ++bound;
        }
    }
    signals_bound_ = (bound > 0);
    spdlog::info("[BoardView] Bound {}/{} nets to live signals",
                 bound, net_names_.size());
}

void BoardView::update_signal_levels(ID3D11DeviceContext* ctx) {
    if (!signals_bound_ || !lvl_buf_) return;

    // Read live signal levels from the pool.
    // Level enum: Low=-1, HiZ=0, High=1. Shader encoding: 0=Low, 1=HiZ, 2=High, 0xFF=unbound.
    bool changed = false;
    for (auto& [net_id, pool_idx] : net_to_pool_) {
        Level lvl = SignalPool::levels[pool_idx];
        uint32_t encoded = static_cast<uint32_t>(static_cast<int8_t>(lvl) + 1);  // -1->0, 0->1, 1->2
        if (net_levels_[net_id] != encoded) {
            net_levels_[net_id] = encoded;
            changed = true;
        }
    }

    if (!changed) return;
    dirty_ = true;

    // Upload to GPU.
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(ctx->Map(lvl_buf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, net_levels_.data(), net_levels_.size() * sizeof(uint32_t));
        ctx->Unmap(lvl_buf_.Get(), 0);
    }
}

// ========================================================================
// ImGui window
// ========================================================================

void BoardView::imgui_window(ID3D11DeviceContext* ctx) {
    if (!open_) return;

    ImGui::SetNextWindowSize(ImVec2(900, 650), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.95f);

    if (!ImGui::Begin("PCB Board View", &open_,
                      ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::End();
        return;
    }

    // --- Toolbar ---
    bool top = (layer_mask_ & 1) != 0;
    bool bot = (layer_mask_ & 2) != 0;
    bool via = (layer_mask_ & 4) != 0;
    bool pad = (layer_mask_ & 8) != 0;
    bool out = (layer_mask_ & 16) != 0;

    ImGui::Checkbox("Top", &top); ImGui::SameLine();
    ImGui::Checkbox("Bot", &bot); ImGui::SameLine();
    ImGui::Checkbox("Via", &via); ImGui::SameLine();
    ImGui::Checkbox("Pad", &pad); ImGui::SameLine();
    ImGui::Checkbox("Outline", &out); ImGui::SameLine();

    uint32_t new_mask = (top ? 1 : 0) | (bot ? 2 : 0) | (via ? 4 : 0) |
                        (pad ? 8 : 0) | (out ? 16 : 0);
    if (new_mask != layer_mask_) { layer_mask_ = new_mask; dirty_ = true; }

    if (ImGui::Button("Fit")) {
        pan_x_ = (bounds_[0] + bounds_[2]) * 0.5f;
        pan_y_ = (bounds_[1] + bounds_[3]) * 0.5f;
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float zx = avail.x / (bounds_[2] - bounds_[0]);
        float zy = avail.y / (bounds_[3] - bounds_[1]);
        zoom_ = (std::min)(zx, zy) * 0.95f;
        dirty_ = true;
    }
    if (highlight_net_) {
        ImGui::SameLine();
        auto it = net_names_.find(highlight_net_);
        if (it != net_names_.end())
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Net: %s", it->second.c_str());
        else
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Net: %u", highlight_net_);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) { highlight_net_ = 0; dirty_ = true; }
    }

    // --- Board image area ---
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int w = (std::max)((int)avail.x, 64);
    int h = (std::max)((int)avail.y, 64);

    // Sample live signal levels (sets dirty_ if anything changed).
    update_signal_levels(ctx);

    // Resize triggers redraw.
    if (w != rt_w_ || h != rt_h_) dirty_ = true;

    if (dirty_) {
        render_to_texture(ctx, w, h);
        dirty_ = false;
    }

    if (rt_srv_) {
        ImVec2 cursor = ImGui::GetCursorScreenPos();

        // Use InvisibleButton to capture all mouse input over the image area,
        // preventing ImGui from moving the window on left-drag.
        ImGui::InvisibleButton("##board_canvas", ImVec2((float)w, (float)h));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();

        // Draw the board image behind the invisible button.
        ImGui::GetWindowDrawList()->AddImage(
            (ImTextureID)rt_srv_.Get(),
            cursor, ImVec2(cursor.x + w, cursor.y + h));

        // --- Mouse interaction ---
        if (hovered) {
            // Zoom with scroll wheel (zoom toward mouse position)
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                ImVec2 mouse = ImGui::GetIO().MousePos;
                float mx = (mouse.x - cursor.x);
                float my = (mouse.y - cursor.y);

                // Board-space position under mouse before zoom
                float bx = pan_x_ + (mx - w * 0.5f) / zoom_;
                float by = pan_y_ + (my - h * 0.5f) / zoom_;

                float factor = (wheel > 0) ? 1.15f : 1.0f / 1.15f;
                zoom_ *= factor;
                zoom_ = std::clamp(zoom_, 0.001f, 100.0f);

                // Adjust pan so board-space point stays under mouse
                pan_x_ = bx - (mx - w * 0.5f) / zoom_;
                pan_y_ = by - (my - h * 0.5f) / zoom_;
                dirty_ = true;
            }

            // Pan with middle mouse drag
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                pan_x_ -= delta.x / zoom_;
                pan_y_ -= delta.y / zoom_;
                dirty_ = true;
            }

            // Also allow left-drag for pan
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                pan_x_ -= delta.x / zoom_;
                pan_y_ -= delta.y / zoom_;
                dirty_ = true;
            }

            // Right-click: highlight net under cursor
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                ImVec2 mouse = ImGui::GetIO().MousePos;
                float mx = (mouse.x - cursor.x);
                float my = (mouse.y - cursor.y);
                float bx = pan_x_ + (mx - w * 0.5f) / zoom_;
                float by = pan_y_ + (my - h * 0.5f) / zoom_;
                uint32_t net = hit_test(bx, by);
                if (net != highlight_net_) { highlight_net_ = net; dirty_ = true; }
            }

            // Tooltip: show net name under cursor
            {
                ImVec2 mouse = ImGui::GetIO().MousePos;
                float mx = (mouse.x - cursor.x);
                float my = (mouse.y - cursor.y);
                float bx = pan_x_ + (mx - w * 0.5f) / zoom_;
                float by = pan_y_ + (my - h * 0.5f) / zoom_;
                uint32_t net = hit_test(bx, by);
                if (net) {
                    auto it = net_names_.find(net);
                    if (it != net_names_.end())
                        ImGui::SetTooltip("%s", it->second.c_str());
                    else
                        ImGui::SetTooltip("Net %u", net);
                }
            }
        }

        // --- Component labels (draw on top at sufficient zoom) ---
        if (zoom_ > 0.5f) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float font_scale = (std::min)(zoom_ * 8.0f, 14.0f);
            if (font_scale >= 5.0f) {
                for (auto& lbl : labels_) {
                    float sx = cursor.x + (lbl.x - pan_x_) * zoom_ + w * 0.5f;
                    float sy = cursor.y + (lbl.y - pan_y_) * zoom_ + h * 0.5f;
                    if (sx >= cursor.x && sx <= cursor.x + w &&
                        sy >= cursor.y && sy <= cursor.y + h) {
                        dl->AddText(nullptr, font_scale,
                                    ImVec2(sx + 2, sy + 2),
                                    IM_COL32(0, 0, 0, 180),
                                    lbl.ref.c_str());
                        dl->AddText(nullptr, font_scale,
                                    ImVec2(sx, sy),
                                    IM_COL32(255, 255, 255, 220),
                                    lbl.ref.c_str());
                    }
                }
            }
        }
    }

    ImGui::End();
}

} // namespace bench
