// EgaRasterizer -- GPU-accelerated EGA display.

#include "display/ega_display.h"
#include "isa/isa_card.h"
#include "isa/isa_ega.h"
#include <d3dcompiler.h>
#include <spdlog/spdlog.h>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace bench {

// Embedded HLSL source compiled at runtime.
static const char EGA_CS_HLSL[] = R"HLSL(

// --- Constant buffer (must match ISA_EGA::GpuConstants) ---
cbuffer EGA_CB : register(b0) {
    uint cursor_ma;      // CRTC R14:R15 (MA units)
    uint cursor_start;   // R10 & 1F
    uint cursor_end;     // R11 & 1F
    uint cursor_blink;   // 1 = cursor visible (~3.75 Hz)
    uint attr_blink;     // 1 = blink-attr chars visible (~1.875 Hz)
    uint _pad0, _pad1, _pad2;
};

// --- Resources ---
Buffer<uint> vram : register(t0);          // 256KB planar VRAM (4 x 64KB)
Buffer<uint> scanline_buf : register(t1);  // 512 scanlines x 280 uint32s

// Stride per scanline in the scanline buffer (uint32s).
// Must match ISA_EGA::ScanlineRegs layout (24 header + 4 x 64 plane rows).
#define SL_STRIDE 280
#define SL_PLANE_OFFSET 24
#define PLANE_U32S 64

RWTexture2D<float4> output_tex : register(u0);  // 912x512 full scan canvas

// Read a byte from the global VRAM buffer (plane p at p * 0x10000).
uint vram_byte(uint addr) {
    uint word = vram[addr >> 2];
    return (word >> ((addr & 3) * 8)) & 0xFF;
}

// Read a byte from a scanline's captured plane row.
uint plane_byte(uint sl_base, uint plane, uint byte_idx) {
    byte_idx &= 0xFF;  // rows are 256 bytes
    uint word = scanline_buf[sl_base + SL_PLANE_OFFSET + plane * PLANE_U32S + (byte_idx >> 2)];
    return (word >> ((byte_idx & 3) * 8)) & 0xFF;
}

// EGA 6-bit rgbRGB color to RGB. Primary bit = 2/3 amplitude, secondary
// bit = 1/3: channel = primary*2 + secondary, scaled by 85.
float4 ega_color(uint c) {
    uint r = (((c >> 2) & 1) * 2 + ((c >> 5) & 1)) * 85;
    uint g = (((c >> 1) & 1) * 2 + ((c >> 4) & 1)) * 85;
    uint b = (((c >> 0) & 1) * 2 + ((c >> 3) & 1)) * 85;
    return float4(r / 255.0, g / 255.0, b / 255.0, 1.0);
}

#define EMIT(c6) { output_tex[dtid.xy] = ega_color(c6); return; }

// =========================================================================
// EGA CRT beam model.
//
// Each thread is a dot at (px, py) in the full scan canvas, anchored at
// the HSYNC leading edge (like a monitor locking to the sync pulse).
// Canvas layout per scanline, in character clocks (dpc dots each):
//   0 .. hsync_width          hsync (black)
//   .. (R0+2 - R4)            back porch, border color
//   .. + (R1+1)               active display (VRAM content)
//   .. R0+2                   front porch, border color
//   beyond R0+2 chars         unused canvas (black)
//
// All rendering-critical register state is captured per scanline by the
// card's beam simulation, including the 4 plane rows the CRTC fetched,
// so mid-frame register changes (split screen, palette raster effects)
// render exactly as the beam saw them.
// =========================================================================

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
    uint px = dtid.x;
    uint py = dtid.y;
    if (px >= 912 || py >= 512) return;

    uint sl_base = py * SL_STRIDE;
    uint attr_mode    = scanline_buf[sl_base + 0];
    uint gc_mode      = scanline_buf[sl_base + 2];
    uint seq_clocking = scanline_buf[sl_base + 3];
    uint crtc_mode    = scanline_buf[sl_base + 4];
    uint ma           = scanline_buf[sl_base + 5];
    uint ra           = scanline_buf[sl_base + 6];
    uint h_disp       = scanline_buf[sl_base + 7];
    uint hsync_pos    = scanline_buf[sl_base + 8];
    uint hsync_width  = scanline_buf[sl_base + 9];
    uint h_total      = scanline_buf[sl_base + 10];
    uint pel_pan      = scanline_buf[sl_base + 11];
    uint plane_en     = scanline_buf[sl_base + 12];
    uint border       = scanline_buf[sl_base + 13];
    uint char_map     = scanline_buf[sl_base + 14];
    uint underline    = scanline_buf[sl_base + 15];

    // Palette entry i (6-bit), packed 4 per uint32 in header words 16-19.
    // (inlined below as pal(i))

    // Character width in dots: 8, or 9 in monochrome text. Dot clock /2
    // doubles every dot (320-wide modes).
    uint char_w = (seq_clocking & 1) ? 8 : 9;
    uint div2 = (seq_clocking >> 3) & 1;
    uint dpc = char_w << div2;

    if (h_total < 4 || h_total > 128 || hsync_pos >= h_total) {
        // CRTC not programmed sensibly yet -- black screen.
        output_tex[dtid.xy] = float4(0, 0, 0, 1);
        return;
    }

    uint total_dots = h_total * dpc;
    uint hsync_dots = hsync_width * dpc;
    uint porch_chars = h_total - hsync_pos - hsync_width;
    if (hsync_pos + hsync_width > h_total) porch_chars = 0;
    uint left_porch_dots = porch_chars * dpc;
    uint active_dots = h_disp * dpc;

    if (px >= total_dots) {
        output_tex[dtid.xy] = float4(0, 0, 0, 1);
        return;
    }

    uint region_px = px;
    if (region_px < hsync_dots) {
        output_tex[dtid.xy] = float4(0, 0, 0, 1);  // hsync: blanked
        return;
    }
    region_px -= hsync_dots;

    if (region_px < left_porch_dots) {
        EMIT(border);
    }
    region_px -= left_porch_dots;

    if (region_px >= active_dots) {
        EMIT(border);  // right overscan to end of line
    }

    // --- Active display ---
    uint eff = (region_px >> div2) + (pel_pan & 7);
    uint word_mode = (crtc_mode & 0x40) ? 0 : 1;
    uint stride = word_mode ? 2 : 1;

    uint pal_idx;
    if (attr_mode & 1) {
        // ----- Graphics -----
        uint col = eff / 8;
        if (gc_mode & 0x20) {
            // Shift register mode: CGA-compatible 2bpp. Each byte pair
            // is chained: pixels 0-3 from the plane 0 byte, 4-7 from
            // the plane 1 byte (2 bits per pixel, MSB first).
            uint b0 = plane_byte(sl_base, 0, col * stride);
            uint b1 = plane_byte(sl_base, 1, col * stride);
            uint pin = eff & 7;
            pal_idx = (pin < 4) ? ((b0 >> (6 - 2 * pin)) & 3)
                                : ((b1 >> (6 - 2 * (pin - 4))) & 3);
        } else {
            // Planar 4bpp: one bit from each plane.
            uint bit = 7 - (eff & 7);
            uint boff = col * stride;
            pal_idx = ((plane_byte(sl_base, 0, boff) >> bit) & 1)
                    | (((plane_byte(sl_base, 1, boff) >> bit) & 1) << 1)
                    | (((plane_byte(sl_base, 2, boff) >> bit) & 1) << 2)
                    | (((plane_byte(sl_base, 3, boff) >> bit) & 1) << 3);
        }
    } else {
        // ----- Alphanumeric -----
        uint col = eff / char_w;
        uint din = eff % char_w;
        uint ch = plane_byte(sl_base, 0, col * stride);
        uint at = plane_byte(sl_base, 1, col * stride);

        uint fg = at & 0x0F;
        uint bg = (at >> 4) & 0x0F;
        if (attr_mode & 8) {
            bg &= 0x07;
            if ((at & 0x80) && !attr_blink)
                fg = bg;
        }

        // Font glyph from plane 2. Character Map Select: bank B (bits
        // 0-1) when attribute bit 3 = 0, bank A (bits 2-3) when 1; each
        // bank is 8K (256 chars x 32 bytes).
        uint bank = (at & 0x08) ? ((char_map >> 2) & 3) : (char_map & 3);
        uint glyph = vram_byte(2 * 0x10000 + bank * 0x2000 + ch * 32 + (ra & 31));

        uint bit;
        if (din < 8) {
            bit = (glyph >> (7 - din)) & 1;
        } else {
            // 9th column: line-graphics chars C0-DF repeat column 8,
            // everything else shows background.
            bit = ((attr_mode & 4) && ch >= 0xC0 && ch <= 0xDF) ? (glyph & 1) : 0;
        }

        // Monochrome-style underline.
        if ((attr_mode & 2) && ((at & 0x07) == 1) && (ra == underline))
            bit = 1;

        // Cursor: MA comparison, blinking, RA within start..end.
        uint cell = ma + col;
        if (cursor_blink && cell == cursor_ma &&
            ra >= cursor_start && ra <= cursor_end)
            bit = 1;

        pal_idx = bit ? fg : bg;
    }

    pal_idx &= plane_en & 0xF;
    uint pal_word = scanline_buf[sl_base + 16 + (pal_idx >> 2)];
    uint color6 = (pal_word >> ((pal_idx & 3) * 8)) & 0x3F;
    EMIT(color6);
}
)HLSL";

const ISA_Card* EgaRasterizer::card() const {
    return ega_card_;
}

Rasterizer::UVRect EgaRasterizer::output_uv_rect() const {
    // Active video starts (R0+2 - R4) chars after the HSYNC leading
    // edge (hsync + back porch), like a real monitor.
    uint32_t h_total = ega_card_->h_total_chars();
    uint32_t hsync_pos = ega_card_->hsync_pos_chars();
    uint32_t dpc = ega_card_->dots_per_char_out();
    uint32_t start_dots = (h_total > hsync_pos) ? (h_total - hsync_pos) * dpc : 0;

    uint32_t view_w = ega_card_->h_displayed_dots();
    if (view_w == 0 || start_dots + view_w > (uint32_t)OUT_W) {
        start_dots = 0;
        view_w = VIEW_W;
    }

    uint32_t top = ega_card_->active_start_scanline();
    uint32_t view_h = ega_card_->v_displayed_lines();
    if (view_h < 100 || view_h > (uint32_t)OUT_H) view_h = VIEW_H;
    if (top + view_h > (uint32_t)OUT_H) top = 0;

    float u0 = float(start_dots) / float(OUT_W);
    float v0 = float(top) / float(OUT_H);
    float u1 = float(start_dots + view_w) / float(OUT_W);
    float v1 = float(top + view_h) / float(OUT_H);
    return { u0, v0, u1, v1 };
}

bool EgaRasterizer::init(const RenderContext& rc) {
    auto* device = rc.device;

    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompile(EGA_CS_HLSL, sizeof(EGA_CS_HLSL), "ega_cs",
                            nullptr, nullptr, "CSMain", "cs_5_0",
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                            &blob, &err);
    if (FAILED(hr)) {
        if (err) spdlog::error("[EGA] shader compile: {}", (char*)err->GetBufferPointer());
        return false;
    }
    hr = device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                     nullptr, &cs_);
    if (FAILED(hr)) { spdlog::error("[EGA] CreateComputeShader failed"); return false; }

    // VRAM buffer (256KB planar, Buffer<uint>)
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = ISA_EGA::VRAM_SIZE;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, &vram_buf_);

        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32_UINT;
        srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv.Buffer.NumElements = ISA_EGA::VRAM_SIZE / 4;
        HRESULT hr2 = device->CreateShaderResourceView(vram_buf_.Get(), &srv, &vram_srv_);
        if (FAILED(hr2)) spdlog::error("[EGA] VRAM SRV failed: 0x{:08X}", (unsigned)hr2);
    }

    // Constant buffer
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = sizeof(ISA_EGA::GpuConstants);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, &cb_);
    }

    // Per-scanline register + plane row buffer
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = ISA_EGA::FRAME_LINES * sizeof(ISA_EGA::ScanlineRegs);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, &scanline_buf_);

        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32_UINT;
        srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv.Buffer.NumElements = ISA_EGA::FRAME_LINES * (sizeof(ISA_EGA::ScanlineRegs) / 4);
        HRESULT hr2 = device->CreateShaderResourceView(scanline_buf_.Get(), &srv, &scanline_srv_);
        if (FAILED(hr2)) spdlog::error("[EGA] Scanline SRV failed: 0x{:08X}", (unsigned)hr2);
    }

    // Output texture (912x512 RGBA8)
    {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = OUT_W;
        td.Height = OUT_H;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        device->CreateTexture2D(&td, nullptr, &out_tex_);

        device->CreateUnorderedAccessView(out_tex_.Get(), nullptr, &out_uav_);
        device->CreateShaderResourceView(out_tex_.Get(), nullptr, &out_srv_);
    }

    spdlog::info("[EGA] GPU rasterizer initialized ({}x{}, viewport {}x{})",
                 OUT_W, OUT_H, VIEW_W, VIEW_H);
    return true;
}

void EgaRasterizer::render(const RenderContext& rc) {
    if (!ega_card_ || !cs_) return;
    auto* ctx = rc.d3d_ctx;
    const auto* card = ega_card_;

    // Upload VRAM (font glyphs are read live from plane 2)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(vram_buf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, card->vram(), ISA_EGA::VRAM_SIZE);
        ctx->Unmap(vram_buf_.Get(), 0);
    }

    // Upload constants
    {
        ISA_EGA::GpuConstants cb;
        card->fill_gpu_constants(cb);

        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(cb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &cb, sizeof(cb));
        ctx->Unmap(cb_.Get(), 0);
    }

    // Upload per-scanline register + plane row snapshots (beam racing)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(scanline_buf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, card->scanline_regs(),
               ISA_EGA::FRAME_LINES * sizeof(ISA_EGA::ScanlineRegs));
        ctx->Unmap(scanline_buf_.Get(), 0);
    }

    // Dispatch
    ctx->CSSetShader(cs_.Get(), nullptr, 0);
    ctx->CSSetConstantBuffers(0, 1, cb_.GetAddressOf());
    ID3D11ShaderResourceView* srvs[] = { vram_srv_.Get(), scanline_srv_.Get() };
    ctx->CSSetShaderResources(0, 2, srvs);
    ID3D11UnorderedAccessView* uavs[1] = { out_uav_.Get() };
    ctx->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);

    ctx->Dispatch((OUT_W + 15) / 16, (OUT_H + 15) / 16, 1);

    // Unbind
    ID3D11UnorderedAccessView* null_uavs[1] = {};
    ctx->CSSetUnorderedAccessViews(0, 1, null_uavs, nullptr);
    ID3D11ShaderResourceView* null_srvs[2] = {};
    ctx->CSSetShaderResources(0, 2, null_srvs);
}

} // namespace bench
