// Renderer -- DX11 + D2D + ImGui overlay.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <ole2.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include <cstring>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include "display/renderer.h"
#include "display/rasterizer.h"
#include "display/mda_display.h"
#include "display/cga_display.h"
#include "display/board_view.h"
#include "isa/isa_cga.h"
#include "core/scheduler.h"
#include "ic/ic_8088.h"
#include "isa/isa_mda.h"
#include "debug/memory_view.h"
#include "test/test_keyboard.h"
#include "isa/isa_fdc.h"
#include "core/save_state.h"
#include <nfd.h>
#include <thread>
#include <filesystem>
#include <fstream>
#include <memory>
#include "ic/ic_8237a.h"
#include "ic/ic_8253.h"
#include "ic/ic_8259a.h"
#include "ic/ic_8284a.h"
#include <d3dcompiler.h>
#include <Zydis/Zydis.h>
#include <spdlog/spdlog.h>
#include <cinttypes>
#include <cmath>
#include <chrono>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using Microsoft::WRL::ComPtr;

namespace bench {

static constexpr int MDA_COLS = 80;
static constexpr int MDA_ROWS = 25;

// ========================================================================
// DX11 + D2D + ImGui state
// ========================================================================

struct DxState {
    HWND hwnd = nullptr;
    int winW = 0, winH = 0;
    float cellW = 0, cellH = 0;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<IDXGISwapChain1> swapChain;
    ComPtr<ID3D11RenderTargetView> rtv;


    // Fullscreen blit resources (for texture-based rasterizers like CGA)
    ComPtr<ID3D11VertexShader> blit_vs;
    ComPtr<ID3D11PixelShader> blit_ps;
    ComPtr<ID3D11SamplerState> blit_sampler;
    ComPtr<ID3D11Buffer> blit_cb;  // UV rect constant buffer

    // CRT scaler intermediate: decoded source (composite or RGBI) rendered
    // 1:1 at dot resolution, then CRT-scaled to the back buffer.
    ComPtr<ID3D11Texture2D> crt_src_tex;
    ComPtr<ID3D11RenderTargetView> crt_src_rtv;
    ComPtr<ID3D11ShaderResourceView> crt_src_srv;
    bool crt_scaler = true;

    // Fullscreen (borderless) state -- Alt+Enter toggle.
    bool fullscreen = false;
    WINDOWPLACEMENT saved_placement = { sizeof(WINDOWPLACEMENT) };
    LONG saved_style = 0;
    bool resize_pending = false;
    int resize_w = 0, resize_h = 0;

    void toggle_fullscreen();
    void request_resize(int w, int h) { resize_pending = true; resize_w = w; resize_h = h; }
    void apply_resize();

    // Active display rasterizer (MDA or CGA, owned by renderer)
    std::unique_ptr<Rasterizer> rasterizer;

    // Bus probe (signal pool indices for bus analyzer)
    const BusProbe* bus_probe = nullptr;
    bool bus_view_open = false;

    // PCB board view
    BoardView board_view;

    // MHz tracking
    const uint64_t* clk_cycles = nullptr;
    uint64_t last_cycles = 0;
    std::chrono::steady_clock::time_point last_time;
    double effective_mhz = 0.0;

    // Debugger
    Scheduler* scheduler = nullptr;
    IC_8088* cpu = nullptr;
    const MemoryView* mem = nullptr;
    IC_8237A* dma = nullptr;
    const IC_8259A* pic = nullptr;
    const IC_8253* pit = nullptr;
    bool* dbg_visible = nullptr;  // points to Renderer::dbg_visible_

    // Zydis disassembler (8086 real mode)
    ZydisDecoder decoder = {};
    ZydisFormatter formatter = {};

    // Disassembly view state (DOSBox-style: persistent top-of-window address,
    // scrolls forward when IP passes the midpoint).
    uint16_t view_cs = 0;
    uint16_t view_ip = 0;
    bool view_init = false;
    bool view_follow = true;   // true = auto-track CS:IP, false = free roam
    int cursor_line = 0;       // free-roam cursor position (disasm line index)
    char view_addr_buf[16] = "0000:0000";

    // Memory viewer
    bool mem_view_open = false;
    char mem_addr_buf[16] = "0000:0000";
    uint32_t mem_view_addr = 0;

    // System window
    bool system_open = false;
    std::string drive_a_path;
    std::string drive_b_path;
    std::string drive_a_loaded;  // path currently loaded in FDC drive 0
    std::string drive_b_loaded;  // path currently loaded in FDC drive 1
    int drop_target_drive = -1;  // -1=none, 0=A, 1=B (set by OLE drag tracking)
    float drive_a_screen_y = 0;  // screen Y of drive A row (for drop targeting)
    float drive_b_screen_y = 0;  // screen Y of drive B row
    ISA_FloppyController* fdc = nullptr;
    const ISA_CGA* cga = nullptr;
    IC_8284A* clk_gen = nullptr;
    SystemInfo sys_info;

    // Save/load state
    Renderer* renderer_owner = nullptr;  // for signaling load requests to main
    bool save_pending = false;
    bool was_running_before_save = false;

    // CGA debug window
    bool cga_debug_open = false;
    ImVec2 cga_pan = ImVec2(0, 0);
    float cga_zoom = 1.0f;

    // Monitor: composite-NTSC filter (CGA only). Reenigne's algorithm
    // (as in 86Box/PCem/DOSBox vid_cga_comp.c): the card side builds a
    // composite waveform from the digital RGBI dot stream via the
    // chroma multiplexer table; the monitor side demodulates it with
    // the decoder phase derived from the color-6 burst waveform.
    bool composite_mode = false;
    ComPtr<ID3D11Buffer> comp_table_buf;               // 1024 x int
    ComPtr<ID3D11ShaderResourceView> comp_table_srv;
    int comp_table[1024] = {};
    float comp_ri = 0, comp_rq = 0, comp_gi = 0, comp_gq = 0;
    float comp_bi = 0, comp_bq = 0;
    bool comp_grayscale = false;
    uint8_t comp_mode_cached = 0xFF;   // last mode reg the table was built for

    void update_composite_table(uint8_t cgamode);

    // Molly guard state for reset
    bool confirm_reset = false;

    // Breakpoint
    char brk_addr_buf[16] = "";
    static constexpr int MEM_ROWS = 16;
    static constexpr int MEM_COLS = 16;

    bool init(HWND hwnd, int w, int h, const ISA_MDA* mda_card);
    void render_display();
    void render_overlay();
    void render_debugger();
    void render_memory_viewer();
    void render_bus_analyzer();
    void render_system_window();
    void render_cga_debug();
    void present();
};

bool DxState::init(HWND hw, int w, int h, const ISA_MDA* mda_card) {
    hwnd = hw; winW = w; winH = h;
    cellW = (float)w / MDA_COLS;
    cellH = (float)h / MDA_ROWS;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        &fl, 1, D3D11_SDK_VERSION, &device, nullptr, &ctx);

    ComPtr<IDXGIDevice1> dxgiDevice;
    device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);
    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = w; scd.Height = h;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    factory->CreateSwapChainForHwnd(device.Get(), hwnd, &scd, nullptr, nullptr, &swapChain);
    // Alt+Enter is handled by us (borderless fullscreen), not DXGI's
    // exclusive-mode transition.
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    // RTV for ImGui (it renders via DX11 directly, not D2D).
    ComPtr<ID3D11Texture2D> backBuf;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuf));
    device->CreateRenderTargetView(backBuf.Get(), nullptr, &rtv);

    // D2D/DWrite now owned by MdaRasterizer (not created here).

    // Initialize active rasterizer
    RenderContext rc_init = { device.Get(), ctx.Get(), winW, winH, cellW, cellH };
    if (rasterizer)
        rasterizer->init(rc_init);

    // Fullscreen blit shaders (for texture-based rasterizers)
    {
        static const char blit_hlsl[] = R"(
            cbuffer BlitCB : register(b0) {
                float4 uv_rect;  // (u0, v0, u1, v1) source rect in texture
                float4 params;   // x=composite, y=tex_w, z=tex_h, w=grayscale decode
                float4 comp0;    // video_ri, video_rq, video_gi, video_gq
                float4 comp1;    // video_bi, video_bq, sharpness, unused
                float4 crt0;     // x=pass mode (2=CRT), y/z=viewport w/h, w=mask scale
            };
            struct VS_OUT { float4 pos : SV_Position; float2 uv : TEXCOORD; };
            VS_OUT VS(uint id : SV_VertexID) {
                VS_OUT o;
                float2 t = float2((id << 1) & 2, id & 2);  // 0..1 fullscreen
                o.uv = uv_rect.xy + t * (uv_rect.zw - uv_rect.xy);
                o.pos = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
                return o;
            }
            Texture2D tex : register(t0);
            SamplerState samp : register(s0);

            // ----- Composite NTSC monitor (reenigne's algorithm) -------
            // Faithful port of vid_cga_comp.c (86Box/PCem/DOSBox).
            //
            // Card side: the CGA's chroma multiplexer output for the
            // transition from dot color L to dot color R at carrier
            // phase (x & 3) is precomputed on the CPU into a 1024-entry
            // table (comp_table[(L<<6)|(R<<2)|phase]), already scaled by
            // monitor contrast/brightness.  One composite sample per
            // 14.318 MHz dot; 912 dots/line = 228 color cycles exactly,
            // and the active area starts on a 4-dot boundary, so the
            // absolute dot x carries the correct carrier phase.
            //
            // Monitor side: comb-style FIR separates chroma (ap/bp) from
            // luma, then I/Q demodulation by quadrant rotation.  The
            // video_ri..video_bq coefficients (comp0/comp1) fold in the
            // YIQ->RGB matrix, saturation, and the decoder phase derived
            // from the color-6 burst waveform in the table itself.
            Buffer<int> comp_table : register(t1);
            Texture2D<uint> idx_tex : register(t2);

            int comp_sample(int x, int y, int w) {
                uint l = idx_tex.Load(int3(clamp(x,     0, w - 1), y, 0));
                uint r = idx_tex.Load(int3(clamp(x + 1, 0, w - 1), y, 0));
                return comp_table[(l << 6) | (r << 2) | (x & 3)];
            }

            float4 PS_composite(VS_OUT pin) {
                int w = (int)params.y;
                int h = (int)params.z;
                int x = clamp((int)(pin.uv.x * params.y), 0, w - 1);
                int y = clamp((int)(pin.uv.y * params.z), 0, h - 1);

                // Composite samples s[k] at dot x + k - 5.
                float s[11];
                [unroll] for (int k = 0; k < 11; k++)
                    s[k] = (float)comp_sample(x + k - 5, y, w);

                float sharp = comp1.z;
                float3 rgb;

                if (params.w > 0.5) {
                    // Luma-only decode: BW bit set, or hires text with
                    // black border (no burst -> monitor drops color).
                    float c = 16.0 * s[5];
                    float d = 8.0 * (s[4] + s[6]);
                    float lum = (c + d) * 256.0 + sharp * (c - d);
                    float g = saturate(lum * (1.0 / 2088960.0));
                    rgb = float3(g, g, g);
                } else {
                    // Chroma bandpass at x-1, x, x+1:
                    //   ap[n] = s[n-4] - 2(s[n-2] - s[n] + s[n+2]) + s[n+4]
                    //   bp[n] = 2(s[n-3] - s[n-1] + s[n+1] - s[n+3])
                    float ap_m = s[0] - 2.0*(s[2] - s[4] + s[6]) + s[8];
                    float ap_0 = s[1] - 2.0*(s[3] - s[5] + s[7]) + s[9];
                    float ap_p = s[2] - 2.0*(s[4] - s[6] + s[8]) + s[10];
                    float bp_0 = 2.0*(s[2] - s[4] + s[6] - s[8]);

                    // Luma = 8*sample - chroma.
                    float lm = 8.0*s[4] - ap_m;
                    float l0 = 8.0*s[5] - ap_0;
                    float lp = 8.0*s[6] - ap_p;
                    float c = 2.0*l0;
                    float d = lm + lp;
                    float lum = (c + d) * 256.0 + sharp * (c - d);

                    // I/Q demodulation: rotate by carrier phase quadrant.
                    float I, Q;
                    int ph = x & 3;
                    if      (ph == 0) { I =  ap_0; Q =  bp_0; }
                    else if (ph == 1) { I = -bp_0; Q =  ap_0; }
                    else if (ph == 2) { I = -ap_0; Q = -bp_0; }
                    else              { I =  bp_0; Q = -ap_0; }

                    rgb = saturate(float3(
                        lum + comp0.x*I + comp0.y*Q,
                        lum + comp0.z*I + comp0.w*Q,
                        lum + comp1.x*I + comp1.y*Q) * (1.0 / 2088960.0));
                }
                return float4(rgb, 1);
            }

            // ----- CRT scaler ------------------------------------------
            // Physically-motivated CRT model, computed in linear light:
            //
            //  * Horizontal: the electron beam is a gaussian spot moving
            //    across the line -- 5-tap gaussian in source dots.
            //  * Vertical: each scanline is a gaussian beam profile whose
            //    width grows with brightness (bright lines bloom, dark
            //    lines show wider gaps) -- the Lottes beam model.
            //  * Aperture grille: RGB phosphor stripes in physical output
            //    pixels. mask scale (crt0.w) is chosen CPU-side from the
            //    actual viewport height so triads stay ~1/360 of screen
            //    height: 1px stripes at 1080p, 2px at 4K UHD.
            //
            // Source is the decoded dot-resolution image (composite or
            // RGBI), so scanline geometry is exact: 200 visible lines.
            float3 crt_fetch(float2 uv) {
                float3 c = tex.SampleLevel(samp, uv, 0).rgb;
                return c * c;  // approximate CRT gamma 2.2 with 2.0 (fast, stable)
            }

            // Gaussian-filtered beam color at line center yline (texels),
            // horizontal beam center src_x (dots).
            float3 crt_hbeam(float src_x, float yline) {
                float v = yline / params.z;
                float xf = floor(src_x - 0.5);
                float3 acc = float3(0, 0, 0);
                float wsum = 0.0;
                [unroll] for (int k = -2; k <= 2; k++) {
                    float xc = xf + k + 0.5;             // tap dot center
                    float d = xc - src_x;                // dots from beam center
                    float w = exp(-d * d * (1.0 / (2.0 * 0.55 * 0.55)));
                    acc += crt_fetch(float2(xc / params.y, v)) * w;
                    wsum += w;
                }
                return acc / wsum;
            }

            float4 PS_crt(VS_OUT pin) {
                float2 src = pin.uv * float2(params.y, params.z);  // dots, lines

                // Two nearest scanlines.
                float ly = src.y - 0.5;
                float l0 = floor(ly);
                float f0 = ly - l0;          // distance from line l0 center

                float3 c0 = crt_hbeam(src.x, l0 + 0.5);
                float3 c1 = crt_hbeam(src.x, l0 + 1.5);

                // Beam profile: gaussian, width scales with line luminance.
                float lum0 = dot(c0, float3(0.299, 0.587, 0.114));
                float lum1 = dot(c1, float3(0.299, 0.587, 0.114));
                float s0 = lerp(0.30, 0.45, saturate(lum0));
                float s1 = lerp(0.30, 0.45, saturate(lum1));
                float w0 = exp(-f0 * f0 / (2.0 * s0 * s0));
                float f1 = 1.0 - f0;
                float w1 = exp(-f1 * f1 / (2.0 * s1 * s1));

                float3 col = c0 * w0 + c1 * w1;

                // Aperture grille (RGB stripes) in physical pixels.
                float mscale = max(crt0.w, 1.0);
                uint stripe = (uint)floor(pin.pos.x / mscale) % 3u;
                const float ml = 0.45;  // off-phosphor leakage
                float3 mask = (stripe == 0u) ? float3(1, ml, ml)
                            : (stripe == 1u) ? float3(ml, 1, ml)
                                             : float3(ml, ml, 1);
                // Normalize mask+scanline energy loss (keeps APL close to
                // the unfiltered image without clipping whites too hard).
                col *= mask * (3.0 / (1.0 + 2.0 * ml)) * 1.10;

                return float4(sqrt(saturate(col)), 1);  // back to gamma
            }

            float4 PS(VS_OUT i) : SV_Target {
                if (crt0.x > 1.5) return PS_crt(i);
                if (params.x > 0.5) return PS_composite(i);
                return tex.Sample(samp, i.uv);
            }
        )";
        ComPtr<ID3DBlob> vs_blob, ps_blob, err;
        D3DCompile(blit_hlsl, sizeof(blit_hlsl), "blit_vs", nullptr, nullptr,
                   "VS", "vs_5_0", 0, 0, &vs_blob, &err);
        D3DCompile(blit_hlsl, sizeof(blit_hlsl), "blit_ps", nullptr, nullptr,
                   "PS", "ps_5_0", 0, 0, &ps_blob, &err);
        if (vs_blob) device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &blit_vs);
        if (ps_blob) device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &blit_ps);

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        device->CreateSamplerState(&sd, &blit_sampler);

        // Constant buffer: uv_rect + params + comp0 + comp1 + crt0
        D3D11_BUFFER_DESC cbd = {};
        cbd.ByteWidth = 80;  // 5 x float4
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&cbd, nullptr, &blit_cb);

        // Composite table buffer (1024 x int, rebuilt on mode change)
        D3D11_BUFFER_DESC tbd = {};
        tbd.ByteWidth = 1024 * sizeof(int);
        tbd.Usage = D3D11_USAGE_DYNAMIC;
        tbd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        tbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&tbd, nullptr, &comp_table_buf);

        D3D11_SHADER_RESOURCE_VIEW_DESC tsrv = {};
        tsrv.Format = DXGI_FORMAT_R32_SINT;
        tsrv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        tsrv.Buffer.NumElements = 1024;
        device->CreateShaderResourceView(comp_table_buf.Get(), &tsrv, &comp_table_srv);

        // CRT scaler intermediate RT (decoded source at dot resolution)
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = CgaRasterizer::OUT_W;
        td.Height = CgaRasterizer::OUT_H;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        device->CreateTexture2D(&td, nullptr, &crt_src_tex);
        device->CreateRenderTargetView(crt_src_tex.Get(), nullptr, &crt_src_rtv);
        device->CreateShaderResourceView(crt_src_tex.Get(), nullptr, &crt_src_srv);
    }

    // ImGui -- scale font + style for high-DPI.
    // Load the font at the scaled pixel size (not FontGlobalScale, which
    // just stretches the already-rasterized atlas and looks blurry).
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    float dpi_scale = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f * dpi_scale);
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device.Get(), ctx.Get());

    last_time = std::chrono::steady_clock::now();

    // Zydis: 8086 real mode disassembler
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_REAL_16, ZYDIS_STACK_WIDTH_16);
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    // PCB board view
    if (!board_view.init(device.Get(), "assets/board_traces.json"))
        return false;

    return true;
}

void DxState::render_display() {
    if (!rasterizer) return;

    RenderContext rc = { device.Get(), ctx.Get(), winW, winH, cellW, cellH };
    rasterizer->render(rc);

    if (!rasterizer->uses_d2d()) {
        // Texture-based rasterizer (CGA): blit output texture to swap chain
        auto* srv = rasterizer->output_srv();
        if (srv && blit_vs && blit_ps) {
            // Clear the back buffer
            float clear[] = { 0, 0, 0, 1 };
            ctx->ClearRenderTargetView(rtv.Get(), clear);

            // Composite is monitor-side but decodes from the card's
            // digital RGBI dot stream.  Rebuild the composite table
            // when the CGA mode/color registers change.
            bool composite_on = cga && composite_mode && rasterizer->index_srv();
            if (composite_on) {
                uint8_t m = cga->mode_register();
                if (m != comp_mode_cached) {
                    comp_mode_cached = m;
                    update_composite_table(m);
                    D3D11_MAPPED_SUBRESOURCE mt;
                    ctx->Map(comp_table_buf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mt);
                    memcpy(mt.pData, comp_table, sizeof(comp_table));
                    ctx->Unmap(comp_table_buf.Get(), 0);
                }
            }

            // Aspect-correct destination viewport: the CGA active area is
            // a 4:3 picture on the monitor. Letterbox/pillarbox to fit.
            float dst_w = (float)winW, dst_h = (float)winH;
            if (dst_w / dst_h > 4.0f / 3.0f) dst_w = dst_h * (4.0f / 3.0f);
            else                             dst_h = dst_w * (3.0f / 4.0f);
            D3D11_VIEWPORT dst_vp = {
                ((float)winW - dst_w) * 0.5f, ((float)winH - dst_h) * 0.5f,
                dst_w, dst_h, 0, 1
            };

            bool crt_on = crt_scaler && crt_src_rtv;
            // Mask scale: keep phosphor triads ~1/360 of picture height.
            // 1px stripes up to 1080p-class, 2px at 1440p, 3px at 4K UHD.
            float mask_scale = (std::max)(1.0f, std::floor(dst_h / 1080.0f + 0.5f));

            auto uv = rasterizer->output_uv_rect();

            // Common pipeline state
            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->IASetInputLayout(nullptr);
            ctx->VSSetShader(blit_vs.Get(), nullptr, 0);
            ctx->VSSetConstantBuffers(0, 1, blit_cb.GetAddressOf());
            ctx->PSSetShader(blit_ps.Get(), nullptr, 0);
            ctx->PSSetConstantBuffers(0, 1, blit_cb.GetAddressOf());
            ctx->PSSetSamplers(0, 1, blit_sampler.GetAddressOf());

            auto upload_cb = [&](float u0, float v0, float u1, float v1,
                                 float comp_flag, float crt_mode, float vw, float vh) {
                D3D11_MAPPED_SUBRESOURCE mapped;
                ctx->Map(blit_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
                float cb_data[20] = {
                    u0, v0, u1, v1,
                    comp_flag,
                    (float)CgaRasterizer::OUT_W,
                    (float)CgaRasterizer::OUT_H,
                    comp_grayscale ? 1.0f : 0.0f,
                    comp_ri, comp_rq, comp_gi, comp_gq,
                    comp_bi, comp_bq, 0.0f /* sharpness */, 0.0f,
                    crt_mode, vw, vh, mask_scale
                };
                memcpy(mapped.pData, cb_data, sizeof(cb_data));
                ctx->Unmap(blit_cb.Get(), 0);
            };

            if (crt_on) {
                // Pass 1: decode source (composite or raw) 1:1 into the
                // dot-resolution intermediate.
                upload_cb(0, 0, 1, 1, composite_on ? 1.0f : 0.0f, 0.0f,
                          (float)CgaRasterizer::OUT_W, (float)CgaRasterizer::OUT_H);
                ctx->OMSetRenderTargets(1, crt_src_rtv.GetAddressOf(), nullptr);
                D3D11_VIEWPORT src_vp = { 0, 0,
                    (float)CgaRasterizer::OUT_W, (float)CgaRasterizer::OUT_H, 0, 1 };
                ctx->RSSetViewports(1, &src_vp);
                ID3D11ShaderResourceView* p1_srvs[3] = {
                    srv, comp_table_srv.Get(), rasterizer->index_srv()
                };
                ctx->PSSetShaderResources(0, 3, p1_srvs);
                ctx->Draw(3, 0);
                ID3D11ShaderResourceView* null3[3] = {};
                ctx->PSSetShaderResources(0, 3, null3);

                // Pass 2: CRT-scale the intermediate to the back buffer.
                upload_cb(uv.u0, uv.v0, uv.u1, uv.v1, 0.0f, 2.0f, dst_w, dst_h);
                ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
                ctx->RSSetViewports(1, &dst_vp);
                ID3D11ShaderResourceView* p2_srvs[1] = { crt_src_srv.Get() };
                ctx->PSSetShaderResources(0, 1, p2_srvs);
                ctx->Draw(3, 0);
                ID3D11ShaderResourceView* null1[1] = {};
                ctx->PSSetShaderResources(0, 1, null1);
            } else {
                // Single pass: decode + crop straight to the back buffer.
                upload_cb(uv.u0, uv.v0, uv.u1, uv.v1,
                          composite_on ? 1.0f : 0.0f, 0.0f, dst_w, dst_h);
                ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
                ctx->RSSetViewports(1, &dst_vp);
                ID3D11ShaderResourceView* ps_srvs[3] = {
                    srv, comp_table_srv.Get(), rasterizer->index_srv()
                };
                ctx->PSSetShaderResources(0, 3, ps_srvs);
                ctx->Draw(3, 0);
                ID3D11ShaderResourceView* null_srvs[3] = {};
                ctx->PSSetShaderResources(0, 3, null_srvs);
            }
        }
    }
    // D2D rasterizers (MDA) already drew to the D2D target directly.
}

// ========================================================================
// Composite table build -- port of reenigne's update_cga16_color()
// (vid_cga_comp.c, 86Box/PCem/DOSBox). Old-style CGA (P/N 1501486,
// the 5150-era card). Monitor knobs fixed at their defaults:
// brightness 0, contrast 100, saturation 100, sharpness 0, hue 0.
// ========================================================================

// Measured chroma multiplexer output: entry [(L&7)<<5 | (R&7)<<2 | phase]
// is the composite level during the transition from color L to color R
// at carrier phase 0-3.
static const unsigned char comp_chroma_multiplexer[256] = {
      2,   2,  2,   2, 114, 174,   4,  3,   2,  1, 133, 135,   2, 113, 150,   4,
    133,   2,  1,  99, 151, 152,   2,  1,   3,  2,  96, 136, 151, 152, 151, 152,
      2,  56, 62,   4, 111, 250, 118,  4,   0, 51, 207, 137,   1, 171, 209,   5,
    140,  50, 54, 100, 133, 202,  57,  4,   2, 50, 153, 149, 128, 198, 198, 135,
     32,   1, 36,  81, 147, 158,   1, 42,  33,  1, 210, 254,  34, 109, 169,  77,
    177,   2,  0, 165, 189, 154,   3, 44,  33,  0,  91, 197, 178, 142, 144, 192,
      4,   2, 61,  67, 117, 151, 112, 83,   4,  0, 249, 255,   3, 107, 249, 117,
    147,   1, 50, 162, 143, 141,  52, 54,   3,  0, 145, 206, 124, 123, 192, 193,
     72,  78,  2,   0, 159, 208,   4,  0,  53, 58, 164, 159,  37, 159, 171,   1,
    248, 117,  4,  98, 212, 218,   5,  2,  54, 59,  93, 121, 176, 181, 134, 130,
      1,  61, 31,   0, 160, 255,  34,  1,   1, 58, 197, 166,   0, 177, 194,   2,
    162, 111, 34,  96, 205, 253,  32,  1,   1, 57, 123, 125, 119, 188, 150, 112,
     78,   4,  0,  75, 166, 180,  20, 38,  78,  1, 143, 246,  42, 113, 156,  37,
    252,   4,  1, 188, 175, 129,   1, 37, 118,  4,  88, 249, 202, 150, 145, 200,
     61,  59, 60,  60, 228, 252, 117, 77,  60, 58, 248, 251,  81, 212, 254, 107,
    198,  59, 58, 169, 250, 251,  81, 80, 100, 58, 154, 250, 251, 252, 252, 252
};

static const double comp_intensity[4] = {
    77.175381, 88.654656, 166.564623, 174.228438
};

void DxState::update_composite_table(uint8_t cgamode) {
    constexpr double tau = 6.28318531;

    // Old CGA: composite = chroma + intensity.
    double min_v = comp_chroma_multiplexer[0] + comp_intensity[0];
    double max_v = comp_chroma_multiplexer[255] + comp_intensity[3];
    double mode_contrast = 256.0 / (max_v - min_v);
    double mode_brightness = -min_v * mode_contrast;
    double mode_hue = ((cgamode & 3) == 1) ? 14.0 : 4.0;  // hires text : other
    double mode_saturation = 2.9;  // saturation 100%, old CGA

    for (int x = 0; x < 1024; ++x) {
        int phase = x & 3;
        int right = (x >> 2) & 15;
        int left  = (x >> 6) & 15;
        int rc = right;
        int lc = left;
        if (cgamode & 0x04) {  // BW bit: chroma forced to 0 or 7
            rc = (right & 8) | ((right & 7) != 0 ? 7 : 0);
            lc = (left & 8) | ((left & 7) != 0 ? 7 : 0);
        }
        double c = comp_chroma_multiplexer[((lc & 7) << 5) | ((rc & 7) << 2) | phase];
        double i = comp_intensity[(left >> 3) | ((right >> 2) & 2)];
        comp_table[x] = (int)((c + i) * mode_contrast + mode_brightness);
    }

    // Decoder phase calibration from the burst: color 6 is the colorburst
    // waveform the CGA emits during blanking.
    double i = comp_table[6 * 68] - comp_table[6 * 68 + 2];
    double q = comp_table[6 * 68 + 1] - comp_table[6 * 68 + 3];

    double a = tau * (33.0 + 90.0 + mode_hue) / 360.0;
    double cs = std::cos(a);
    double sn = std::sin(a);
    double r = 256.0 * mode_saturation / std::sqrt(i * i + q * q);

    double iq_adjust_i = -(i * cs + q * sn) * r;
    double iq_adjust_q = (q * cs - i * sn) * r;

    constexpr double ri = 0.9563, rq = 0.6210;
    constexpr double gi = -0.2721, gq = -0.6474;
    constexpr double bi = -1.1069, bq = 1.7046;

    comp_ri = (float)(int)(ri * iq_adjust_i + rq * iq_adjust_q);
    comp_rq = (float)(int)(-ri * iq_adjust_q + rq * iq_adjust_i);
    comp_gi = (float)(int)(gi * iq_adjust_i + gq * iq_adjust_q);
    comp_gq = (float)(int)(-gi * iq_adjust_q + gq * iq_adjust_i);
    comp_bi = (float)(int)(bi * iq_adjust_i + bq * iq_adjust_q);
    comp_bq = (float)(int)(-bi * iq_adjust_q + bq * iq_adjust_i);

    // Luma-only decode on the BW bit only (no colorburst in the encoded
    // signal).  MartyPC -- the compatibility reference for 8088 MPH --
    // does not model a monitor color-killer for the no-burst 80-col
    // text case (black border), and always decodes chroma otherwise.
    comp_grayscale = (cgamode & 0x04) != 0;

    spdlog::info("[Composite] table rebuilt: mode=0x{:02X} hue={} {}",
                 cgamode, mode_hue, comp_grayscale ? "(grayscale)" : "");
}

void DxState::render_overlay() {
    // Update MHz calculation every frame.
    if (clk_cycles) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_time).count();
        if (elapsed >= 0.5) {  // update every 500ms for stability
            uint64_t cur = *clk_cycles;
            double delta = static_cast<double>(cur - last_cycles);
            effective_mhz = (delta / elapsed) / 1e6;
            last_cycles = cur;
            last_time = now;
        }
    }

    // ImGui overlay
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Deferred save (paused last frame, clock thread now parked)
    if (save_pending && scheduler && cpu) {
        save_pending = false;
        std::filesystem::create_directories("saves");
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        struct tm lt;
        localtime_s(&lt, &tt);
        char fname[64];
        std::snprintf(fname, sizeof(fname), "saves/bench_%04d%02d%02d_%02d%02d%02d.b51",
                      lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                      lt.tm_hour, lt.tm_min, lt.tm_sec);
        bench::save_system(*scheduler, cpu, clk_gen, fname);
        if (was_running_before_save) scheduler->resume();
    }

    // F12 toggles debugger panel.
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false) && dbg_visible)
        *dbg_visible = !*dbg_visible;

    // --- Right-click context menu (same as bottom-right Menu) ---
    if (!ImGui::GetIO().WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        ImGui::OpenPopup("MainMenu");

    if (ImGui::BeginPopup("MainMenu")) {
        if (ImGui::MenuItem("System"))        system_open = !system_open;
        if (ImGui::MenuItem("Board"))         board_view.toggle();
        if (ImGui::MenuItem("Debugger"))      { if (dbg_visible) *dbg_visible = !*dbg_visible; }
        if (ImGui::MenuItem("Bus"))           bus_view_open = !bus_view_open;
        if (ImGui::MenuItem("Memory"))        mem_view_open = !mem_view_open;
        if (cga && ImGui::MenuItem("CGA"))   cga_debug_open = !cga_debug_open;
        ImGui::EndPopup();
    }

    // --- Status + Menu HUD (bottom-right) ---
    {
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing;

        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::SetNextWindowPos(
            ImVec2((float)winW - 10.0f, (float)winH - 10.0f),
            ImGuiCond_Always,
            ImVec2(1.0f, 1.0f));

        ImGui::Begin("##stats", nullptr, flags);

        // Menu button (centered)
        {
            float btn_w = ImGui::CalcTextSize("  Menu  ").x + ImGui::GetStyle().FramePadding.x * 2;
            float avail = ImGui::GetContentRegionAvail().x;
            if (avail > btn_w) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btn_w) * 0.5f);
            if (ImGui::Button("  Menu  "))
                ImGui::OpenPopup("MainMenu");

            if (ImGui::BeginPopup("MainMenu")) {
                if (ImGui::MenuItem("System"))        system_open = !system_open;
                if (ImGui::MenuItem("Board"))         board_view.toggle();
                if (ImGui::MenuItem("Debugger"))      { if (dbg_visible) *dbg_visible = !*dbg_visible; }
                if (ImGui::MenuItem("Bus"))           bus_view_open = !bus_view_open;
                if (ImGui::MenuItem("Memory"))        mem_view_open = !mem_view_open;
                if (cga && ImGui::MenuItem("CGA"))   cga_debug_open = !cga_debug_open;
                ImGui::EndPopup();
            }
        }

        // Status: PAUSED or MHz (clickable to toggle, centered)
        bool paused = scheduler && scheduler->is_paused();
        {
            const char* label = paused ? "PAUSED" : nullptr;
            char mhz_buf[32];
            if (!paused) snprintf(mhz_buf, sizeof(mhz_buf), "%.2f MHz", effective_mhz);
            const char* text = paused ? "PAUSED" : mhz_buf;
            float tw = ImGui::CalcTextSize(text).x;
            float av = ImGui::GetContentRegionAvail().x;
            if (av > tw) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (av - tw) * 0.5f);
        }
        if (paused)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "PAUSED");
        else
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%.2f MHz", effective_mhz);
        if (scheduler && ImGui::IsItemClicked()) {
            if (paused) scheduler->resume(); else scheduler->pause();
            if (dbg_visible) *dbg_visible = !paused;
        }
        ImGui::End();
    }

    // --- Debugger panel (top-left, translucent) ---
    if (dbg_visible && *dbg_visible && scheduler)
        render_debugger();

    // --- Memory viewer (separate window) ---
    if (mem_view_open && mem)
        render_memory_viewer();

    // --- Bus analyzer (separate window) ---
    if (bus_view_open && bus_probe)
        render_bus_analyzer();

    // --- System window ---
    if (system_open)
        render_system_window();

    // --- CGA debug window ---
    if (cga_debug_open && cga)
        render_cga_debug();

    // --- PCB board view (F2 toggle) ---
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
        board_view.toggle();
    board_view.imgui_window(ctx.Get());

    ImGui::Render();

    ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DxState::render_debugger() {
    static constexpr int DISASM_LINES = 21;  // total visible lines
    static constexpr int MID_LINE = DISASM_LINES / 2;  // IP target row

    // Layout:  1 toolbar (buttons) + 1 toolbar (break/t-state/dma/dump)
    //        + 1 separator + 1 CLK + 3 regs + 1 flags/PIC + 1 PIT
    //        + 1 separator + 1 view addr + DISASM_LINES = DISASM_LINES + 11
    // Plus title bar + frame padding.
    static constexpr int CONTENT_LINES = DISASM_LINES + 11;
    // "F000:FFFF  FF FF FF FF FF FF  mov word [bp+si+0x1234], 0x5678"
    // = ~60 chars.  Consolas at 14px base: char width ~ 8.4px * dpi_scale.
    static constexpr int LINE_CHARS = 62;

    ImGuiWindowFlags dbg_flags =
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize;

    float line_h = ImGui::GetTextLineHeightWithSpacing();
    float char_w = ImGui::CalcTextSize("X").x;
    float pad = ImGui::GetStyle().WindowPadding.x * 2;
    float title_h = ImGui::GetFrameHeight();  // title bar
    float w = char_w * LINE_CHARS + pad;
    float h = line_h * CONTENT_LINES + title_h + pad;

    ImGui::SetNextWindowBgAlpha(0.90f);
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(w, h));

    ImGui::Begin("Debugger [`]", dbg_visible, dbg_flags);

    bool paused = scheduler->is_paused();

    // --- Toolbar ---
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (paused) scheduler->resume(); else scheduler->pause();
        paused = !paused;
    }
    {
        float btn_w = ImGui::CalcTextSize("Resume (F5)").x + ImGui::GetStyle().FramePadding.x * 2;
        if (paused) {
            if (ImGui::Button("Resume (F5)", ImVec2(btn_w, 0))) scheduler->resume();
        } else {
            if (ImGui::Button("Pause  (F5)", ImVec2(btn_w, 0))) scheduler->pause();
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!paused);
    // F9: Step Over -- run until next instruction at same stack level
    bool do_step_over = ImGui::Button("Over (F9)");
    if (paused && ImGui::IsKeyPressed(ImGuiKey_F9, true))
        do_step_over = true;
    if (do_step_over && cpu && mem) {
        uint16_t cs = cpu->regs16_ro()[IC_8088::CS];
        uint16_t ip = cpu->ip();
        uint16_t sp = cpu->regs16_ro()[IC_8088::SP];
        uint32_t phys = ((uint32_t)cs << 4) + ip;
        uint8_t buf[15];
        mem->read(phys & 0xFFFFF, buf, 15);
        ZydisDecodedInstruction instr;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        uint16_t len = 1;
        if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buf, 15, &instr, operands)))
            len = (uint16_t)instr.length;
        scheduler->step_over(ip + len, sp);
    }
    ImGui::SameLine();
    // F10: Step Into -- single instruction
    bool do_step_instr = ImGui::Button("Into (F10)");
    if (paused && ImGui::IsKeyPressed(ImGuiKey_F10, true))
        do_step_instr = true;
    if (do_step_instr)
        scheduler->step_instruction();
    ImGui::SameLine();
    // F11: Cycle -- single CLK cycle
    bool do_step_cycle = ImGui::Button("Cycle (F11)");
    if (paused && ImGui::IsKeyPressed(ImGuiKey_F11, true))
        do_step_cycle = true;
    if (do_step_cycle)
        scheduler->step_cycle();
    ImGui::EndDisabled();

    // --- Second toolbar row: breakpoint, T-state, DMA, dumps ---
    ImGui::Text("Break:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_w * 11);
    if (ImGui::InputText("##brk", brk_addr_buf, sizeof(brk_addr_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        unsigned seg = 0, off = 0;
        uint32_t linear = UINT32_MAX;
        if (sscanf(brk_addr_buf, "%x:%x", &seg, &off) == 2)
            linear = ((seg << 4) + off) & 0xFFFFF;
        else if (sscanf(brk_addr_buf, "%x", &off) == 1)
            linear = off & 0xFFFFF;
        scheduler->set_break_address(linear);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("seg:off or linear. Enter to set.");

    if (!cpu) { ImGui::End(); return; }

    {
        static const char* t_names[] = {"Ti", "T1", "T2", "T3", "Tw", "T4"};
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
            t_names[static_cast<int>(cpu->t_state())]);
    }
    if (dma) {
        static const char* dma_names[] = {
            "SI", "BREQ", "S1", "S2", "S3", "S4",
            "M2M-S1", "M2M-S2", "M2M-S3", "M2M-S4"
        };
        ImGui::SameLine();
        const char* ds = dma_names[static_cast<int>(dma->state())];
        int ch = dma->active_channel();
        if (ch >= 0)
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DMA:%s/CH%d", ds, ch);
        else
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DMA:%s", ds);
    }
    // Dump buttons (same row, right side)
    if (paused) {
        if (cga) {
            ImGui::SameLine();
            if (ImGui::Button("Dump VRAM")) {
                FILE* f = fopen("cga_vram.bin", "wb");
                if (f) {
                    fwrite(cga->vram(), 1, ISA_CGA::FB_SIZE, f);
                    fclose(f);
                    spdlog::info("[CGA] VRAM dumped to cga_vram.bin ({} bytes)", ISA_CGA::FB_SIZE);
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Write 16KB CGA VRAM (0xB8000) to cga_vram.bin");
        }
        if (cpu && mem) {
            ImGui::SameLine();
            if (ImGui::Button("Dump Seg")) {
                uint16_t dump_cs = view_follow ? cpu->regs16_ro()[IC_8088::CS] : view_cs;
                uint32_t base = (uint32_t)dump_cs << 4;
                uint8_t seg_buf[0x10000];
                mem->read(base & 0xFFFFF, seg_buf, 0x10000);
                char fname[64];
                snprintf(fname, sizeof(fname), "seg_%04X.bin", dump_cs);
                FILE* f = fopen(fname, "wb");
                if (f) {
                    fwrite(seg_buf, 1, 0x10000, f);
                    fclose(f);
                    spdlog::info("[DBG] Segment {:04X} dumped to {} (64KB from {:05X}h)",
                                 dump_cs, fname, base);
                }
            }
            if (ImGui::IsItemHovered()) {
                uint16_t dump_cs = view_follow ? cpu->regs16_ro()[IC_8088::CS] : view_cs;
                ImGui::SetTooltip("Dump %04X (64KB from %05Xh) to seg_%04X.bin",
                                  dump_cs, (uint32_t)dump_cs << 4, dump_cs);
            }
        }
    }

    ImGui::Separator();
    if (clk_cycles)
        ImGui::Text("CLK: %" PRIu64 "  %.2f MHz", *clk_cycles, effective_mhz);

    const uint16_t* r = cpu->regs16_ro();
    const uint8_t* r8 = cpu->regs8_ro();
    using R = IC_8088::Reg16;
    using F = IC_8088::Flag;

    // --- Registers (segment:offset pairs) ---
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
    ImGui::Text("AX=%04X  BX=%04X  CX=%04X  DX=%04X  BP=%04X",
                 r[R::AX], r[R::BX], r[R::CX], r[R::DX], r[R::BP]);
    ImGui::Text("CS:IP=%04X:%04X  DS:SI=%04X:%04X",
                 r[R::CS], cpu->ip(), r[R::DS], r[R::SI]);
    ImGui::Text("SS:SP=%04X:%04X  ES:DI=%04X:%04X",
                 r[R::SS], r[R::SP], r[R::ES], r[R::DI]);

    char fl[] = "---------";
    if (r8[F::OF]) fl[0] = 'O'; if (r8[F::DF]) fl[1] = 'D';
    if (r8[F::IF]) fl[2] = 'I'; if (r8[F::TF]) fl[3] = 'T';
    if (r8[F::SF]) fl[4] = 'S'; if (r8[F::ZF]) fl[5] = 'Z';
    if (r8[F::AF]) fl[6] = 'A'; if (r8[F::PF]) fl[7] = 'P';
    if (r8[F::CF]) fl[8] = 'C';
    ImGui::Text("%s", fl);
    if (pic) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
            "  IRR=%02X ISR=%02X IMR=%02X", pic->irr(), pic->isr(), pic->imr());
    }
    ImGui::PopStyleColor();
    if (pit) {
        auto ch = pit->channel_info(0);
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
            "PIT0: M%d cnt=%04X rl=%04X OUT=%d gate=%d %s%s",
            ch.mode, ch.count, ch.reload, ch.out, ch.gate,
            ch.counting ? "run" : "stop", ch.null_count ? " null" : "");
    }

    // --- Disassembly (DOSBox-style persistent view) ---
    if (!mem || !paused) { ImGui::End(); return; }

    ImGui::Separator();

    uint16_t cs = r[R::CS];
    uint16_t ip = cpu->ip();

    // Initialize view to CS:IP on first frame.
    if (!view_init) {
        view_cs = cs;
        view_ip = ip;
        view_follow = true;
        view_init = true;
        snprintf(view_addr_buf, sizeof(view_addr_buf), "%04X:%04X", cs, ip);
    }

    // View address bar: [View: ____:____] [CS:IP]
    ImGui::Text("View:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_w * 11);
    if (ImGui::InputText("##view", view_addr_buf, sizeof(view_addr_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        unsigned vseg = 0, voff = 0;
        if (sscanf(view_addr_buf, "%x:%x", &vseg, &voff) == 2) {
            view_cs = (uint16_t)vseg;
            view_ip = (uint16_t)voff;
            view_follow = false;
            cursor_line = 0;
        } else if (sscanf(view_addr_buf, "%x", &voff) == 1) {
            view_cs = (uint16_t)(voff >> 4);
            view_ip = (uint16_t)(voff & 0xF);
            view_follow = false;
            cursor_line = 0;
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("seg:off or linear. Enter to jump.");
    ImGui::SameLine();
    if (ImGui::Button("CS:IP")) {
        view_cs = cs;
        view_ip = ip;
        view_follow = true;
        snprintf(view_addr_buf, sizeof(view_addr_buf), "%04X:%04X", cs, ip);
    }
    if (!view_follow) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "(free)");
    }

    // In follow mode, re-sync view when CS changes or IP jumps out of view.
    if (view_follow && view_cs != cs) {
        view_cs = cs;
        view_ip = ip;
    }

    uint32_t cs_base = (uint32_t)view_cs << 4;

    // --- Disassemble helper lambda ---
    struct DisLine { uint16_t addr; uint8_t len; char hex[32]; char text[128]; };
    DisLine lines[DISASM_LINES];
    int ip_line = -1;
    bool ip_in_view_seg = (view_cs == cs);

    auto disassemble_view = [&]() {
        uint16_t cur = view_ip;
        ip_line = -1;
        for (int i = 0; i < DISASM_LINES; ++i) {
            lines[i].addr = cur;
            if (ip_in_view_seg && cur == ip) ip_line = i;

            uint8_t buf[15];
            mem->read((cs_base + cur) & 0xFFFFF, buf, 15);

            ZydisDecodedInstruction instr;
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
            if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buf, 15, &instr, operands))) {
                ZydisFormatterFormatInstruction(&formatter, &instr, operands,
                    instr.operand_count, lines[i].text, sizeof(lines[i].text),
                    (uint64_t)cs_base + cur, ZYAN_NULL);
                lines[i].len = (uint8_t)instr.length;
                int hpos = 0;
                for (int b = 0; b < (int)instr.length && hpos < 28; ++b)
                    hpos += snprintf(lines[i].hex + hpos, 32 - hpos, "%02X ", buf[b]);
            } else {
                snprintf(lines[i].hex, 32, "%02X", buf[0]);
                snprintf(lines[i].text, 128, "db 0x%02X", buf[0]);
                lines[i].len = 1;
            }
            cur += lines[i].len;
        }
    };

    disassemble_view();

    // In follow mode, scroll the view so IP stays near the middle.
    if (view_follow && ip_in_view_seg) {
        bool need_redisasm = false;
        if (ip_line < 0) {
            view_ip = ip;
            need_redisasm = true;
        } else if (ip_line > MID_LINE) {
            int scroll = ip_line - MID_LINE;
            for (int i = 0; i < scroll; ++i)
                view_ip += lines[i].len;
            need_redisasm = true;
        }
        if (need_redisasm)
            disassemble_view();
    }

    // --- Keyboard / mouse navigation ---
    int scroll_lines = 0;
    bool arrow_down = false, arrow_up = false;

    if (ImGui::IsWindowFocused()) {
        arrow_down = ImGui::IsKeyPressed(ImGuiKey_DownArrow, true);
        arrow_up   = ImGui::IsKeyPressed(ImGuiKey_UpArrow, true);

        if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true)) {
            if (view_follow) { view_follow = false; cursor_line = MID_LINE; }
            scroll_lines += DISASM_LINES - 2;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true)) {
            if (view_follow) { view_follow = false; cursor_line = MID_LINE; }
            scroll_lines -= DISASM_LINES - 2;
        }
    }
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
        scroll_lines += -(int)ImGui::GetIO().MouseWheel;
        if (view_follow) { view_follow = false; cursor_line = MID_LINE; }
    }

    // Arrow keys: move cursor, scroll at edges
    if (arrow_down) {
        if (view_follow) { view_follow = false; cursor_line = ip_line >= 0 ? ip_line : MID_LINE; }
        cursor_line++;
        if (cursor_line >= DISASM_LINES) { cursor_line = DISASM_LINES - 1; scroll_lines += 1; }
    }
    if (arrow_up) {
        if (view_follow) { view_follow = false; cursor_line = ip_line >= 0 ? ip_line : MID_LINE; }
        cursor_line--;
        if (cursor_line < 0) { cursor_line = 0; scroll_lines -= 1; }
    }

    if (scroll_lines != 0) {
        view_follow = false;
        int wheel = scroll_lines;
        if (wheel > 0) {
            // Scroll forward: advance view_ip by 'wheel' instruction lengths.
            uint16_t scan = view_ip;
            for (int i = 0; i < wheel; ++i) {
                uint8_t sb[15];
                mem->read((cs_base + scan) & 0xFFFFF, sb, 15);
                ZydisDecodedInstruction si;
                ZydisDecodedOperand so[ZYDIS_MAX_OPERAND_COUNT];
                if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, sb, 15, &si, so)))
                    scan += si.length;
                else
                    scan += 1;
            }
            view_ip = scan;
        } else {
            // Scroll backward: try disassembling from (view_ip - N) for several
            // candidate offsets and pick the stream that naturally lands on view_ip.
            int rows = -wheel;
            // Scan back far enough that we can find 'rows' instructions before view_ip.
            // Max x86 instruction = 15 bytes, so back up by rows*15 + some margin.
            int backtrack = rows * 15 + 30;
            if (backtrack > (int)view_ip) backtrack = (int)view_ip;
            uint16_t best_start = view_ip;  // fallback: don't move
            int best_lines = 0;
            // Try each possible start offset and see which one reaches view_ip exactly.
            for (int off = backtrack; off >= 1; --off) {
                uint16_t scan = view_ip - (uint16_t)off;
                int count = 0;
                bool hit = false;
                while (scan < view_ip && count < 256) {
                    uint8_t sb[15];
                    mem->read((cs_base + scan) & 0xFFFFF, sb, 15);
                    ZydisDecodedInstruction si;
                    ZydisDecodedOperand so[ZYDIS_MAX_OPERAND_COUNT];
                    uint16_t step = 1;
                    if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, sb, 15, &si, so)))
                        step = si.length;
                    scan += step;
                    ++count;
                }
                if (scan == view_ip && count >= rows) {
                    // Walk this stream again to find the address 'rows' instructions before view_ip.
                    scan = view_ip - (uint16_t)off;
                    int total = count;
                    int skip = total - rows;
                    for (int s = 0; s < skip; ++s) {
                        uint8_t sb[15];
                        mem->read((cs_base + scan) & 0xFFFFF, sb, 15);
                        ZydisDecodedInstruction si;
                        ZydisDecodedOperand so[ZYDIS_MAX_OPERAND_COUNT];
                        if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, sb, 15, &si, so)))
                            scan += si.length;
                        else
                            scan += 1;
                    }
                    if (count > best_lines) {
                        best_lines = count;
                        best_start = scan;
                    }
                }
            }
            view_ip = best_start;
        }
        disassemble_view();
    }

    // --- Render ---
    // Clamp cursor
    if (cursor_line < 0) cursor_line = 0;
    if (cursor_line >= DISASM_LINES) cursor_line = DISASM_LINES - 1;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < DISASM_LINES; ++i) {
        bool is_ip = ip_in_view_seg && (lines[i].addr == ip);
        bool is_cursor = !view_follow && (i == cursor_line);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;

        // Background: IP gets yellow fill, cursor gets cyan outline (can overlap)
        if (is_ip) {
            dl->AddRectFilled(
                ImVec2(pos.x - 4, pos.y),
                ImVec2(pos.x + w + 4, pos.y + line_h),
                IM_COL32(60, 60, 20, 220));
        }
        if (is_cursor) {
            dl->AddRect(
                ImVec2(pos.x - 4, pos.y),
                ImVec2(pos.x + w + 4, pos.y + line_h),
                IM_COL32(80, 180, 220, 200));
        }

        // Text color: IP = bright yellow, cursor = bright, default = dim
        ImVec4 col = is_ip     ? ImVec4(1.0f, 1.0f, 0.3f, 1.0f)
                   : is_cursor ? ImVec4(0.7f, 0.85f, 0.7f, 1.0f)
                   :             ImVec4(0.50f, 0.65f, 0.50f, 1.0f);
        ImGui::TextColored(col,
            "%04X:%04X  %-18s %s", view_cs, lines[i].addr,
            lines[i].hex, lines[i].text);
    }

    ImGui::End();
}

void DxState::render_memory_viewer() {
    ImGui::SetNextWindowBgAlpha(0.92f);

    if (!ImGui::Begin("Memory", &mem_view_open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // Address input: accepts "SSSS:OOOO" or "XXXXX" (linear)
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputText("##addr", mem_addr_buf, sizeof(mem_addr_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        unsigned seg = 0, off = 0;
        if (sscanf(mem_addr_buf, "%x:%x", &seg, &off) == 2)
            mem_view_addr = ((seg << 4) + off) & 0xFFFFF;
        else if (sscanf(mem_addr_buf, "%x", &off) == 1)
            mem_view_addr = off & 0xFFFFF;
    }
    ImGui::SameLine();
    // Quick-jump buttons
    if (cpu) {
        const uint16_t* r = cpu->regs16_ro();
        if (ImGui::Button("SS:SP")) {
            mem_view_addr = ((r[IC_8088::SS] << 4) + r[IC_8088::SP]) & 0xFFFFF;
            snprintf(mem_addr_buf, sizeof(mem_addr_buf), "%04X:%04X",
                     r[IC_8088::SS], r[IC_8088::SP]);
        }
        ImGui::SameLine();
        if (ImGui::Button("DS:SI")) {
            mem_view_addr = ((r[IC_8088::DS] << 4) + r[IC_8088::SI]) & 0xFFFFF;
            snprintf(mem_addr_buf, sizeof(mem_addr_buf), "%04X:%04X",
                     r[IC_8088::DS], r[IC_8088::SI]);
        }
        ImGui::SameLine();
        if (ImGui::Button("ES:DI")) {
            mem_view_addr = ((r[IC_8088::ES] << 4) + r[IC_8088::DI]) & 0xFFFFF;
            snprintf(mem_addr_buf, sizeof(mem_addr_buf), "%04X:%04X",
                     r[IC_8088::ES], r[IC_8088::DI]);
        }
        ImGui::SameLine();
        if (ImGui::Button("CS:IP")) {
            mem_view_addr = ((r[IC_8088::CS] << 4) + cpu->ip()) & 0xFFFFF;
            snprintf(mem_addr_buf, sizeof(mem_addr_buf), "%04X:%04X",
                     r[IC_8088::CS], cpu->ip());
        }
    }

    // Mouse wheel scrolling (1 row = 16 bytes)
    float wheel = ImGui::GetIO().MouseWheel;
    if (ImGui::IsWindowHovered() && wheel != 0.0f) {
        int delta = -(int)wheel * MEM_COLS;
        mem_view_addr = (mem_view_addr + delta) & 0xFFFFF;
    }

    ImGui::Separator();

    // Header
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
        "         00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII");

    // Rows
    for (int row = 0; row < MEM_ROWS; ++row) {
        uint32_t row_addr = (mem_view_addr + row * MEM_COLS) & 0xFFFFF;
        uint8_t data[16];
        mem->read(row_addr, data, MEM_COLS);

        char hex[64];
        int p = 0;
        for (int i = 0; i < 16; ++i) {
            if (i == 8) hex[p++] = ' ';
            p += snprintf(hex + p, sizeof(hex) - p, "%02X ", data[i]);
        }

        char ascii[17];
        for (int i = 0; i < 16; ++i)
            ascii[i] = (data[i] >= 0x20 && data[i] < 0x7F) ? (char)data[i] : '.';
        ascii[16] = '\0';

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
            "%05X  %s %s", row_addr, hex, ascii);
    }

    ImGui::End();
}

void DxState::render_bus_analyzer() {
    const auto& bp = *bus_probe;
    const auto* L = SignalPool::levels;

    // Helper: read N contiguous levels as hex value (High=1 bit set)
    auto bus_hex = [&](int base, int n) -> uint32_t {
        uint32_t v = 0;
        for (int i = 0; i < n; ++i)
            if (L[base + i] == Level::High) v |= (1u << i);
        return v;
    };
    // Helper: signal level as char
    auto lch = [&](int idx) -> char {
        Level v = L[idx];
        return v == Level::High ? 'H' : v == Level::Low ? 'L' : 'Z';
    };
    // Helper: active-low signal as colored text
    auto active_low = [&](const char* name, int idx) {
        Level v = L[idx];
        bool active = (v == Level::Low);
        ImVec4 col = active ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        ImGui::TextColored(col, "%s=%c", name, lch(idx));
    };

    ImGui::SetNextWindowBgAlpha(0.92f);

    if (!ImGui::Begin("Bus Analyzer", &bus_view_open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImVec4 grn = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    ImVec4 dim = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImVec4 yel = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
    ImVec4 red = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
    ImVec4 cyn = ImVec4(0.4f, 1.0f, 1.0f, 1.0f);

    // Address/data buses
    uint32_t la_val = bus_hex(bp.la, 20);
    uint32_t ad_val = bus_hex(bp.ad, 8);
    uint32_t d_val  = bus_hex(bp.d, 8);
    uint32_t xd_val = bus_hex(bp.xd, 8);
    uint32_t md_val = bus_hex(bp.md, 8);

    // --- Summary line: decode bus cycle type ---
    {
        // S2 S1 S0 decode (active-low pins: L on pin = 0 in value)
        //  0  0  0 = INTA       0  0  1 = IOR
        //  0  1  0 = IOW        0  1  1 = HALT
        //  1  0  0 = FETCH      1  0  1 = MEMR
        //  1  1  0 = MEMW       1  1  1 = passive
        int s2 = (L[bp.s2] == Level::Low) ? 0 : 1;
        int s1 = (L[bp.s1] == Level::Low) ? 0 : 1;
        int s0 = (L[bp.s0] == Level::Low) ? 0 : 1;
        int code = (s2 << 2) | (s1 << 1) | s0;
        static const char* cycle_names[] = {
            "INTA", "IOR", "IOW", "HALT", "FETCH", "MEMR", "MEMW", "---"
        };
        const char* cycle = cycle_names[code];
        bool is_read = (code == 1 || code == 4 || code == 5);  // IOR, FETCH, MEMR
        bool is_write = (code == 2 || code == 6);               // IOW, MEMW
        bool is_active = (code < 7);

        if (is_active) {
            uint8_t data = is_read ? (uint8_t)d_val : (uint8_t)d_val;
            ImGui::TextColored(cyn, "%s  %05X  D=%02X", cycle, la_val, data);
            if (is_read) {
                ImGui::SameLine();
                ImGui::TextColored(cyn, " (reading)");
            } else if (is_write) {
                ImGui::SameLine();
                ImGui::TextColored(red, " (writing)");
            }
        } else {
            bool dma_owns = L[bp.aen_brd] == Level::High;
            ImGui::TextColored(dim, "%s  %s", dma_owns ? "DMA owns bus" : "idle", cycle);
        }
        ImGui::Separator();
    }

    // Last completed bus transaction (from CPU)
    if (cpu) {
        static const char* tx_names[] = {
            "INTA", "IOR", "IOW", "HALT", "FETCH", "MEMR", "MEMW", "---"
        };
        auto& tx = cpu->last_bus_tx();
        ImGui::TextColored(yel, "Last: %-5s [%05X] = %02X",
                           tx_names[tx.type & 7], tx.addr, tx.data);
        ImGui::Separator();
    }

    ImGui::TextColored(grn, "LA[19:0]=%05X   AD[7:0]=%02X", la_val, ad_val);
    ImGui::TextColored(grn, " D[7:0] =%02X     XD[7:0]=%02X    MD[7:0]=%02X", d_val, xd_val, md_val);

    ImGui::Separator();

    // 8288 bus control
    ImGui::Text("8288:");
    ImGui::SameLine(); active_low("~MEMR", bp.memr);
    ImGui::SameLine(); active_low("~MEMW", bp.memw);
    ImGui::SameLine(); active_low("~IOR", bp.ior);
    ImGui::SameLine(); active_low("~IOW", bp.iow);

    ImGui::Text("     ");
    ImGui::SameLine(); active_low("ALE", bp.ale);   // active high actually
    ImGui::SameLine(); active_low("~DEN", bp.den);
    ImGui::SameLine();
    ImGui::TextColored(dim, "DT/~R=%c", lch(bp.dtr));

    ImGui::Separator();

    // CPU status
    ImGui::Text("CPU: ");
    ImGui::SameLine();
    ImGui::TextColored(grn, "~S2=%c ~S1=%c ~S0=%c", lch(bp.s2), lch(bp.s1), lch(bp.s0));
    ImGui::SameLine();
    ImGui::TextColored(dim, " READY=%c", lch(bp.ready));

    ImGui::Separator();

    // DMA
    bool hrq_active = L[bp.hrq] == Level::High;
    bool holda_active = L[bp.holda] == Level::High;
    bool aen_active = L[bp.aen_brd] == Level::High;
    ImGui::TextColored(hrq_active ? yel : dim, "HRQ=%c", lch(bp.hrq));
    ImGui::SameLine();
    ImGui::TextColored(holda_active ? yel : dim, "HOLDA=%c", lch(bp.holda));
    ImGui::SameLine();
    ImGui::TextColored(aen_active ? yel : dim, "AEN_BRD=%c", lch(bp.aen_brd));
    ImGui::SameLine();
    active_low("~AEN", bp.aen_bar);

    ImGui::Text("DRQ: %c%c%c%c  ~DACK: %c%c%c%c",
        lch(bp.drq0), lch(bp.drq1), lch(bp.drq2), lch(bp.drq3),
        lch(bp.dack0), lch(bp.dack1), lch(bp.dack2), lch(bp.dack3));

    ImGui::Separator();

    // Interrupts
    ImGui::TextColored(L[bp.intr] == Level::High ? yel : dim, "INTR=%c", lch(bp.intr));
    ImGui::SameLine();
    ImGui::TextColored(L[bp.nmi] == Level::High ? yel : dim, "NMI=%c", lch(bp.nmi));
    ImGui::SameLine();
    ImGui::TextColored(dim, "CLK=%c  RESET=%c", lch(bp.clk), lch(bp.reset));

    ImGui::End();
}

void DxState::render_system_window() {
    if (!ImGui::Begin("System", &system_open, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    ImVec4 grn = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    ImVec4 dim = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    ImVec4 red = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

    // --- Machine info ---
    ImGui::TextColored(grn, "IBM PC 5150");
    ImGui::TextColored(dim, "Model B (64KB-256KB System Board)");
    ImGui::TextColored(dim, "CPU: Intel 8088 @ %.2f MHz", effective_mhz);
    int total_kb = 256 + sys_info.expansion_kb;
    if (sys_info.expansion_kb > 0)
        ImGui::TextColored(dim, "RAM: %d KB (256 KB planar + %d KB expansion)", total_kb, sys_info.expansion_kb);
    else
        ImGui::TextColored(dim, "RAM: 256 KB planar DRAM");
    ImGui::TextColored(dim, "Display: %s", cga ? "CGA" : "MDA");
    ImGui::TextColored(dim, "ROM: GLABIOS 0.4.1");

    // --- Monitor (CGA only): composite vs RGBI ---
    if (cga) {
        ImGui::Separator();
        ImGui::TextColored(grn, "Monitor");
        if (ImGui::Checkbox("Composite mode", &composite_mode)) {
            spdlog::info("[System] CGA composite mode {}", composite_mode ? "ON" : "OFF");
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Simulate an NTSC composite monitor.\n"
                              "Color bleed and hi-res text artifact colors.");
        if (composite_mode) {
            ImGui::TextColored(dim, "mode=%02X -> %s",
                cga->mode_register(),
                comp_grayscale ? "grayscale (BW bit)" : "color");
        }
        ImGui::Checkbox("CRT scaler", &crt_scaler);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Scanline beam + aperture grille phosphor mask.\n"
                              "Mask scales with output resolution (4K-aware).");
        ImGui::TextColored(dim, "Alt+Enter: fullscreen");
    }

    // --- ISA Expansion Slots ---
    ImGui::Separator();
    ImGui::TextColored(grn, "ISA Slots");
    for (int i = 0; i < 5; i++) {
        const auto& slot = sys_info.slots[i];
        if (!slot.ref.empty()) {
            if (!slot.card.empty())
                ImGui::TextColored(dim, "  %s: %s", slot.ref.c_str(), slot.card.c_str());
            else
                ImGui::TextColored(dim, "  %s: (empty)", slot.ref.c_str());
        }
    }

    // --- Reset ---
    ImGui::Separator();
    if (clk_gen) {
        if (ImGui::Button("Reset"))
            confirm_reset = true;

        // Molly guard: Reset confirmation
        if (confirm_reset)
            ImGui::OpenPopup("Confirm Reset");
        if (ImGui::BeginPopupModal("Confirm Reset", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextColored(red, "Are you sure you want to reset?");
            ImGui::Text("This pulses the RESET line.");
            ImGui::Separator();
            if (ImGui::Button("Yes, Reset", ImVec2(140, 0))) {
                spdlog::info("[System] Reset requested by user");
                if (scheduler) scheduler->resume();
                clk_gen->psu_reset();
                confirm_reset = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(140, 0))) {
                confirm_reset = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // --- Save / Load State ---
    ImGui::Separator();
    ImGui::TextColored(grn, "Save State");
    if (ImGui::Button("Save State")) {
        was_running_before_save = scheduler && !scheduler->is_paused();
        if (was_running_before_save) scheduler->pause();
        save_pending = true;  // executes next frame after clock thread parks
    }
    ImGui::SameLine();
    if (ImGui::Button("Load State") && renderer_owner) {
        nfdchar_t* out = nullptr;
        nfdresult_t result = NFD_OpenDialog("b51", "saves", &out);
        if (result == NFD_OKAY && out) {
            renderer_owner->load_path_ = out;
            renderer_owner->load_requested_.store(true, std::memory_order_release);
            free(out);
        }
    }

    ImGui::Separator();
    ImGui::TextColored(grn, "Floppy Drives");

    // --- Drive A: ---
    auto drive_row = [&](const char* label, int drive_idx, std::string& path, float& screen_y) {
        // Store screen Y for drop targeting
        screen_y = ImGui::GetCursorScreenPos().y;

        // Highlight row when dragging a file over this drive
        bool highlight = (drop_target_drive == drive_idx);
        if (highlight) {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            float h = ImGui::GetTextLineHeightWithSpacing();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(pos.x - 4, pos.y),
                ImVec2(pos.x + w + 4, pos.y + h),
                IM_COL32(40, 80, 40, 180));
        }

        ImGui::Text("%s", label);
        ImGui::SameLine();

        // Truncate display path to filename
        std::string display = path.empty() ? "(empty)" : path;
        size_t slash = display.find_last_of("/\\");
        if (slash != std::string::npos) display = display.substr(slash + 1);
        if (display.size() > 30) display = "..." + display.substr(display.size() - 27);

        ImGui::TextColored(highlight ? ImVec4(1,1,0,1) : (path.empty() ? dim : grn), "%s", display.c_str());

        ImGui::SameLine();
        char btn_id[16];
        snprintf(btn_id, sizeof(btn_id), "...##%s", label);
        if (ImGui::Button(btn_id)) {
            nfdchar_t* out = nullptr;
            nfdresult_t result = NFD_OpenDialog("img", nullptr, &out);
            if (result == NFD_OKAY && out) {
                path = out;
                free(out);
                spdlog::info("[System] {} = {}", label, path);
            }
        }
        ImGui::SameLine();
        char eject_id[16];
        snprintf(eject_id, sizeof(eject_id), "Eject##%s", label);
        if (ImGui::Button(eject_id)) {
            path.clear();
            spdlog::info("[System] {} ejected", label);
        }
    };

    drive_row("A:", 0, drive_a_path, drive_a_screen_y);
    drive_row("B:", 1, drive_b_path, drive_b_screen_y);

    // Hot-swap drives
    auto swap_drive = [&](int drive_idx, std::string& path, std::string& loaded, const char* label) {
        if (!fdc || path == loaded) return;
        loaded = path;
        if (path.empty()) {
            fdc->load_image({}, 9, 2, drive_idx);
            spdlog::info("[System] Drive {}: ejected", label);
        } else {
            std::ifstream f(path, std::ios::binary | std::ios::ate);
            if (f) {
                auto sz = f.tellg();
                std::vector<uint8_t> img(static_cast<size_t>(sz));
                f.seekg(0);
                f.read(reinterpret_cast<char*>(img.data()), sz);
                int spt = 9, hds = 2;
                if (sz > 400000) { spt = 15; hds = 2; }
                if (sz > 1300000) { spt = 18; hds = 2; }
                fdc->load_image(std::move(img), spt, hds, drive_idx);
                spdlog::info("[System] Drive {}: loaded {} ({} bytes, {}spt/{}hd)",
                             label, path, (int)sz, spt, hds);
            } else {
                spdlog::warn("[System] Drive {}: failed to open {}", label, path);
            }
        }
    };
    swap_drive(0, drive_a_path, drive_a_loaded, "A:");
    swap_drive(1, drive_b_path, drive_b_loaded, "B:");

    ImGui::Separator();
    ImGui::TextColored(dim, "Drop .img files onto drive labels to mount.");

    ImGui::End();
}

void DxState::render_cga_debug() {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("CGA Debug", &cga_debug_open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    ImVec4 grn = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    ImVec4 cyn = ImVec4(0.4f, 1.0f, 1.0f, 1.0f);
    ImVec4 yel = ImVec4(1.0f, 1.0f, 0.4f, 1.0f);
    ImVec4 dim = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

    // --- CRTC Register State ---
    const uint8_t* r = cga->crtc_regs();
    uint8_t mode = cga->mode_register();
    uint8_t color = cga->color_register();

    if (ImGui::CollapsingHeader("CRTC Registers", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Columns(4, "crtc_regs", true);
        ImGui::TextColored(grn, "R0  H Total");     ImGui::NextColumn();
        ImGui::Text("%3d", r[0]);                     ImGui::NextColumn();
        ImGui::TextColored(grn, "R1  H Displayed");  ImGui::NextColumn();
        ImGui::Text("%3d", r[1]);                     ImGui::NextColumn();

        ImGui::TextColored(grn, "R2  H Sync Pos");  ImGui::NextColumn();
        ImGui::Text("%3d", r[2]);                     ImGui::NextColumn();
        ImGui::TextColored(grn, "R3  Sync Width");   ImGui::NextColumn();
        ImGui::Text("%3d (H:%d)", r[3], r[3] & 0xF); ImGui::NextColumn();

        ImGui::TextColored(grn, "R4  V Total");      ImGui::NextColumn();
        ImGui::Text("%3d", r[4] & 0x7F);             ImGui::NextColumn();
        ImGui::TextColored(grn, "R5  V Adjust");     ImGui::NextColumn();
        ImGui::Text("%3d", r[5] & 0x1F);             ImGui::NextColumn();

        ImGui::TextColored(grn, "R6  V Displayed");  ImGui::NextColumn();
        ImGui::Text("%3d", r[6] & 0x7F);             ImGui::NextColumn();
        ImGui::TextColored(grn, "R7  V Sync Pos");   ImGui::NextColumn();
        ImGui::Text("%3d", r[7] & 0x7F);             ImGui::NextColumn();

        ImGui::TextColored(grn, "R8  Interlace");    ImGui::NextColumn();
        ImGui::Text("%3d", r[8]);                     ImGui::NextColumn();
        ImGui::TextColored(grn, "R9  Max Scanline"); ImGui::NextColumn();
        ImGui::Text("%3d", r[9] & 0x1F);             ImGui::NextColumn();

        ImGui::TextColored(grn, "R12:13 Start Addr");ImGui::NextColumn();
        ImGui::Text("0x%04X", (r[12] << 8) | r[13]); ImGui::NextColumn();
        ImGui::TextColored(grn, "R14:15 Cursor");    ImGui::NextColumn();
        ImGui::Text("0x%04X", (r[14] << 8) | r[15]); ImGui::NextColumn();
        ImGui::Columns(1);
    }

    // --- Mode & Color ---
    if (ImGui::CollapsingHeader("Mode / Color", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextColored(cyn, "Mode 0x%02X:", mode);
        ImGui::SameLine();
        if (mode & 0x01) ImGui::TextColored(yel, "+HRES "); else ImGui::TextColored(dim, "-hres ");
        ImGui::SameLine();
        if (mode & 0x02) ImGui::TextColored(yel, "+GFX ");  else ImGui::TextColored(dim, "-gfx ");
        ImGui::SameLine();
        if (mode & 0x04) ImGui::TextColored(yel, "+BW ");   else ImGui::TextColored(dim, "-bw ");
        ImGui::SameLine();
        if (mode & 0x08) ImGui::TextColored(yel, "+EN ");   else ImGui::TextColored(dim, "-en ");
        ImGui::SameLine();
        if (mode & 0x10) ImGui::TextColored(yel, "+1BPP "); else ImGui::TextColored(dim, "-1bpp ");
        ImGui::SameLine();
        if (mode & 0x20) ImGui::TextColored(yel, "+BLINK"); else ImGui::TextColored(dim, "-blink");

        ImGui::TextColored(cyn, "Color 0x%02X:", color);
        ImGui::SameLine();
        ImGui::Text("Border=%d  Palette=%d  Bright=%d",
                     color & 0xF, (color >> 5) & 1, (color >> 4) & 1);
    }

    // Per-scanline state (used by frame buffer hover below).
    const auto* sl = cga->scanline_regs();

    ImGui::TextColored(dim, "Active start: scanline %d", cga->active_start_scanline());

    // --- Full Frame Texture ---
    if (ImGui::CollapsingHeader("Frame Buffer (912x262)", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Zoom controls
        ImGui::SliderFloat("Zoom", &cga_zoom, 0.5f, 8.0f, "%.1fx");

        // Get the output texture SRV from the rasterizer
        auto* srv = rasterizer ? rasterizer->output_srv() : nullptr;
        if (srv) {
            // CGA pixel aspect ratio: the CRT displays 912x262 dots in 4:3.
            // PAR = (912/262) / (4/3) = 2.614.  Each dot is ~2.6x taller than wide.
            static constexpr float PAR = (912.0f / 262.0f) / (4.0f / 3.0f);
            float tex_w = CgaRasterizer::OUT_W * cga_zoom;
            float tex_h = CgaRasterizer::OUT_H * cga_zoom * PAR;

            // Scrollable child region for panning
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImGui::BeginChild("CGA_Frame", ImVec2(avail.x, avail.y - 80), true,
                              ImGuiWindowFlags_HorizontalScrollbar);

            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)srv, ImVec2(tex_w, tex_h));

            // --- Beam position overlay (electron gun indicator) ---
            {
                uint32_t beam_sl = cga->beam_scanline();
                uint32_t beam_hcc = cga->beam_hcc();
                uint32_t beam_dot = cga->beam_dot();
                bool hires = (cga->mode_register() & 0x01) ||
                             (cga->mode_register() & 0x10);
                uint32_t dpc = hires ? 8 : 16;
                float bx = (float)(beam_hcc * dpc + beam_dot) * cga_zoom;
                float by = (float)beam_sl * cga_zoom * PAR;

                ImDrawList* dl = ImGui::GetWindowDrawList();
                float cx = cursor.x + bx;
                float cy = cursor.y + by;
                float arm = 12.0f;  // crosshair arm length in screen pixels

                // Bright magenta crosshair + center dot
                ImU32 col = IM_COL32(255, 0, 255, 255);
                dl->AddLine(ImVec2(cx - arm, cy), ImVec2(cx + arm, cy), col, 1.5f);
                dl->AddLine(ImVec2(cx, cy - arm), ImVec2(cx, cy + arm), col, 1.5f);
                dl->AddCircleFilled(ImVec2(cx, cy), 3.0f, col);

                // Infer port 3DA status from beam state
                const uint8_t* cr = cga->crtc_regs();
                uint8_t status_3da = 0;
                if (beam_hcc >= cr[ISA_CGA::CRTC_HDISPLAYED] ||
                    cga->beam_vcc() >= cr[ISA_CGA::CRTC_VDISPLAYED])
                    status_3da |= 0x01;
                if (cga->beam_in_vsync())
                    status_3da |= 0x08;

                // Label with scanline/HCC and 3DA value
                char beam_label[96];
                snprintf(beam_label, sizeof(beam_label),
                         "SL:%u HCC:%u VCC:%u DOT:%u  3DA=%02Xh%s%s",
                         beam_sl, beam_hcc, cga->beam_vcc(), beam_dot,
                         status_3da,
                         (status_3da & 0x01) ? " ~DE" : " DE",
                         (status_3da & 0x08) ? " VSYNC" : "");
                dl->AddText(ImVec2(cx + 6, cy - 14), col, beam_label);
            }

            // Hover info: show scanline data at mouse position
            if (ImGui::IsItemHovered()) {
                ImVec2 mouse = ImGui::GetMousePos();
                int mx = (int)((mouse.x - cursor.x) / cga_zoom);
                int my = (int)((mouse.y - cursor.y) / (cga_zoom * PAR));
                if (mx >= 0 && mx < CgaRasterizer::OUT_W &&
                    my >= 0 && my < CgaRasterizer::OUT_H) {
                    const auto& s = sl[my];

                    // Compute beam position info
                    uint32_t h_total_chars = (r[0] > 0) ? r[0] + 1 : 114;
                    uint32_t char_w = 912 / h_total_chars;
                    if (char_w == 0) char_w = 8;
                    uint32_t left_porch = h_total_chars - s.hsync_pos - s.hsync_width;
                    uint32_t left_dots = left_porch * char_w;

                    int active_x = mx - (int)left_dots;
                    int char_col = (active_x >= 0) ? active_x / (int)char_w : -1;

                    ImGui::BeginTooltip();
                    ImGui::Text("Dot: %d  Scanline: %d", mx, my);
                    ImGui::Separator();
                    ImGui::TextColored(grn, "VCC=%d  RA=%d  MA=0x%04X", s.vcc, s.ra, s.ma);
                    ImGui::TextColored(cyn, "Mode=0x%02X  Color=0x%02X", s.mode, s.color);
                    ImGui::TextColored(yel, "H_Disp=%d  V_Disp=%d", s.h_displayed, s.v_displayed);
                    ImGui::TextColored(dim, "HSync=%d+%d  HTotal=%d",
                                       s.hsync_pos, s.hsync_width, r[0]);
                    if (char_col >= 0 && char_col < (int)s.h_displayed) {
                        // Read from the VRAM row captured in the scanline buffer
                        const uint8_t* row = reinterpret_cast<const uint8_t*>(s.vram_row);
                        if (s.mode & 0x02) {
                            // Graphics mode
                            ImGui::TextColored(grn, "Active px=%d  byte[%d]=0x%02X",
                                               active_x, active_x / 8,
                                               row[active_x / 8]);
                        } else {
                            // Text mode
                            uint8_t ch = row[char_col * 2];
                            uint8_t attr = row[char_col * 2 + 1];
                            ImGui::TextColored(grn, "Col=%d  Char=0x%02X '%c'  Attr=0x%02X",
                                               char_col, ch,
                                               (ch >= 32 && ch < 127) ? ch : '.',
                                               attr);
                            ImGui::TextColored(dim, "  FG=%d  BG=%d", attr & 0xF, (attr >> 4) & 0xF);
                        }
                    } else {
                        ImGui::TextColored(dim, "Overscan/Blank (col=%d)", char_col);
                    }
                    ImGui::EndTooltip();
                }
            }

            ImGui::EndChild();
        } else {
            ImGui::TextColored(dim, "(no CGA output texture available)");
        }
    }

    ImGui::End();
}

void DxState::present() {
    swapChain->Present(1, 0);
}

// Alt+Enter: borderless fullscreen on the window's current monitor.
void DxState::toggle_fullscreen() {
    fullscreen = !fullscreen;
    if (fullscreen) {
        saved_style = GetWindowLongW(hwnd, GWL_STYLE);
        GetWindowPlacement(hwnd, &saved_placement);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hwnd, HWND_TOP,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED);
        spdlog::info("[Renderer] Fullscreen ON ({}x{})",
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top);
    } else {
        SetWindowLongW(hwnd, GWL_STYLE, saved_style);
        SetWindowPlacement(hwnd, &saved_placement);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        spdlog::info("[Renderer] Fullscreen OFF");
    }
}

// Deferred WM_SIZE: resize swap chain between frames (RTV must be unbound).
void DxState::apply_resize() {
    if (!resize_pending) return;
    resize_pending = false;
    int w = resize_w, h = resize_h;
    if (!swapChain || w <= 0 || h <= 0 || (w == winW && h == winH)) return;

    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    rtv.Reset();
    HRESULT hr = swapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        spdlog::error("[Renderer] ResizeBuffers failed: 0x{:08X}", (unsigned)hr);
        return;
    }
    ComPtr<ID3D11Texture2D> backBuf;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuf));
    device->CreateRenderTargetView(backBuf.Get(), nullptr, &rtv);

    winW = w; winH = h;
    cellW = (float)w / MDA_COLS;
    cellH = (float)h / MDA_ROWS;

    // D2D rasterizers (MDA) render into views of the old back buffer --
    // rebuild them against the new one.
    if (rasterizer && rasterizer->uses_d2d()) {
        RenderContext rc = { device.Get(), ctx.Get(), winW, winH, cellW, cellH };
        rasterizer->init(rc);
    }
}

// ========================================================================
// Window
// ========================================================================

static TestKeyboard* s_kbd = nullptr;  // set by render_loop before window creation
static DxState* s_dx = nullptr;       // for drop handler

// OLE IDropTarget for live drag highlighting + drop onto drive rows.
class DropTarget : public IDropTarget {
    LONG ref_ = 1;
    HWND hwnd_;
    bool has_files(IDataObject* obj) {
        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        return obj->QueryGetData(&fmt) == S_OK;
    }
    int drive_from_y(float y) {
        if (!s_dx || s_dx->drive_b_screen_y <= 0) return 0;
        float dist_a = std::abs(y - s_dx->drive_a_screen_y);
        float dist_b = std::abs(y - s_dx->drive_b_screen_y);
        return (dist_b < dist_a) ? 1 : 0;
    }
public:
    DropTarget(HWND hwnd) : hwnd_(hwnd) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_IDropTarget) { *ppv = this; AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override { auto r = InterlockedDecrement(&ref_); if (!r) delete this; return r; }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* obj, DWORD, POINTL pt, DWORD* effect) override {
        if (!has_files(obj)) { *effect = DROPEFFECT_NONE; return S_OK; }
        *effect = DROPEFFECT_COPY;
        if (s_dx) {
            s_dx->system_open = true;
            POINT cp = { pt.x, pt.y };
            ScreenToClient(hwnd_, &cp);
            s_dx->drop_target_drive = drive_from_y((float)cp.y);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL pt, DWORD* effect) override {
        *effect = DROPEFFECT_COPY;
        if (s_dx) {
            POINT cp = { pt.x, pt.y };
            ScreenToClient(hwnd_, &cp);
            s_dx->drop_target_drive = drive_from_y((float)cp.y);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        if (s_dx) s_dx->drop_target_drive = -1;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* obj, DWORD, POINTL pt, DWORD* effect) override {
        *effect = DROPEFFECT_NONE;
        if (!has_files(obj) || !s_dx) return S_OK;

        POINT cp = { pt.x, pt.y };
        ScreenToClient(hwnd_, &cp);
        int drive = drive_from_y((float)cp.y);

        FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM stg;
        if (SUCCEEDED(obj->GetData(&fmt, &stg))) {
            HDROP hDrop = (HDROP)stg.hGlobal;
            wchar_t path[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
                char utf8[MAX_PATH * 3];
                WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr);
                auto& target = (drive == 1) ? s_dx->drive_b_path : s_dx->drive_a_path;
                const char* label = (drive == 1) ? "B:" : "A:";
                target = utf8;
                spdlog::info("[System] Drop -> {} {}", label, target);
            }
            ReleaseStgMedium(&stg);
            *effect = DROPEFFECT_COPY;
        }
        s_dx->drop_target_drive = -1;
        return S_OK;
    }
};

static LRESULT CALLBACK RendererWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    // F10 generates WM_SYSKEYDOWN -- don't let DefWindowProc eat it for
    // menu activation, otherwise F10 requires two presses.
    if ((msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && wp == VK_F10)
        return 0;

    // Alt+Enter: toggle borderless fullscreen. Swallow the key entirely
    // so it neither reaches the emulated keyboard nor beeps.
    if (msg == WM_SYSKEYDOWN && wp == VK_RETURN) {
        if (!(lp & (1 << 30)) && s_dx)   // ignore auto-repeat
            s_dx->toggle_fullscreen();
        return 0;
    }
    if (msg == WM_SYSKEYUP && wp == VK_RETURN)
        return 0;

    // Track client size changes (fullscreen toggle, user resize).
    // The swap chain resize is applied between frames.
    if (msg == WM_SIZE && s_dx && wp != SIZE_MINIMIZED) {
        s_dx->request_resize(LOWORD(lp), HIWORD(lp));
        return 0;
    }

    // Forward keyboard events to the emulated keyboard.
    // Only when ImGui doesn't want keyboard input (not typing in a text field).
    if (s_kbd && ImGui::GetCurrentContext() && !ImGui::GetIO().WantCaptureKeyboard
        && s_dx && s_dx->scheduler && !s_dx->scheduler->is_paused()) {
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            uint8_t xt = TestKeyboard::vk_to_xt((int)wp);
            if (xt) {
                s_kbd->inject_key(xt);
            }
        } else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
            uint8_t xt = TestKeyboard::vk_to_xt((int)wp);
            if (xt) {
                s_kbd->inject_key(xt | 0x80);
            }
        }
    }

    // WM_DROPFILES handled by OLE IDropTarget (DropTarget class above).
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void Renderer::render_loop(std::stop_token stop) {
    s_kbd = kbd_;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int monW = GetSystemMetrics(SM_CXSCREEN);
    int monH = GetSystemMetrics(SM_CYSCREEN);
    int winH = monH * 2 / 3;
    int winW = winH * 4 / 3;
    int x = (monW - winW) / 2;
    int y = (monH - winH) / 2;

    RECT wr = { 0, 0, winW, winH };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = RendererWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"BenchRenderer";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"BenchRenderer", L"IBM 5150 - MDA",
        WS_OVERLAPPEDWINDOW, x, y,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    // OLE drag-drop for live highlighting during drag
    OleInitialize(nullptr);
    auto* dropTarget = new DropTarget(hwnd);
    RegisterDragDrop(hwnd, dropTarget);
    dropTarget->Release();  // RegisterDragDrop AddRef'd

    DxState dx;
    s_dx = &dx;
    dx.clk_cycles = clk_cycles_;
    dx.scheduler = scheduler_;
    dx.cpu = cpu_;
    dx.mem = mem_;
    dx.dma = dma_;
    dx.pic = pic_;
    dx.pit = pit_;
    dx.bus_probe = bus_probe_;
    dx.dbg_visible = &dbg_visible_;
    dx.drive_a_path = disk_a_path_;
    dx.drive_a_loaded = disk_a_path_;
    dx.fdc = fdc_;
    dx.cga = cga_;
    dx.clk_gen = clk_gen_;
    dx.sys_info = sys_info_;
    dx.renderer_owner = this;
    // Create rasterizer based on installed display card
    if (cga_)
        dx.rasterizer = std::make_unique<CgaRasterizer>(cga_);
    else if (vram_)
        dx.rasterizer = std::make_unique<MdaRasterizer>(mda_card_, vram_);
    if (!dx.init(hwnd, winW, winH, mda_card_)) {
        spdlog::error("[Renderer] Failed to init DX11");
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    running_.store(true);
    ready_.count_down();

    MSG msg = {};
    while (!stop.stop_requested() && msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Pick up pending board signal binding from main thread.
            if (brd_map_ready_.load(std::memory_order_acquire)) {
                dx.board_view.bind_signals(pending_brd_map_);
                brd_map_ready_.store(false, std::memory_order_release);
            }

            dx.apply_resize();
            dx.render_display();
            dx.render_overlay();
            dx.present();
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    s_dx = nullptr;
    RevokeDragDrop(hwnd);
    OleUninitialize();
    DestroyWindow(hwnd);
    running_.store(false);
}

void Renderer::set_disk_a_path(const std::string& path) { disk_a_path_ = path; }

std::string Renderer::take_pending_load() {
    if (!load_requested_.load(std::memory_order_acquire)) return {};
    load_requested_.store(false, std::memory_order_release);
    return std::move(load_path_);
}

void Renderer::start(const uint8_t* vram, const uint64_t* clk_cycles,
                     Scheduler* scheduler, IC_8088* cpu,
                     const MemoryView* mem, IC_8237A* dma,
                     const ISA_MDA* mda_card,
                     const BusProbe* bus,
                     TestKeyboard* kbd,
                     ISA_FloppyController* fdc,
                     const ISA_CGA* cga,
                     IC_8284A* clk_gen,
                     const SystemInfo& sys_info,
                     const IC_8259A* pic,
                     const IC_8253* pit) {
    vram_ = vram;
    clk_cycles_ = clk_cycles;
    scheduler_ = scheduler;
    cpu_ = cpu;
    mem_ = mem;
    dma_ = dma;
    pic_ = pic;
    pit_ = pit;
    mda_card_ = mda_card;
    bus_probe_ = bus;
    cga_ = cga;
    kbd_ = kbd;
    fdc_ = fdc;
    clk_gen_ = clk_gen;
    sys_info_ = sys_info;
    thread_ = std::jthread([this](std::stop_token stop) {
        render_loop(stop);
    });
    ready_.wait();
}

void Renderer::bind_board_signals(const std::unordered_map<std::string, int>& brd_map) {
    pending_brd_map_ = brd_map;
    brd_map_ready_.store(true, std::memory_order_release);
}

void Renderer::stop() {
    if (thread_.joinable()) {
        thread_.request_stop();
        if (running_.load())
            PostThreadMessage(GetThreadId(thread_.native_handle()), WM_QUIT, 0, 0);
        thread_.join();
    }
}

} // namespace bench
