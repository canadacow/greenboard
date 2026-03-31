// CgaRasterizer -- GPU-accelerated CGA display.

#include "display/cga_display.h"
#include "isa/isa_card.h"
#include "isa/isa_cga.h"
#include <d3dcompiler.h>
#include <spdlog/spdlog.h>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace bench {

// Embedded HLSL source compiled at runtime.
static const char CGA_CS_HLSL[] = R"HLSL(

// --- Constant buffer: CGA register state (must match GpuConstants struct) ---
cbuffer CGA_CB : register(b0) {
    uint mode;           // 0x3D8
    uint color;          // 0x3D9
    uint cursor_blink;   // 1 = cursor visible (~3.75 Hz)
    uint attr_blink;     // 1 = blink-attr chars visible (~1.875 Hz)
    uint composite;      // 1 = composite decode (TODO)
    uint start_addr;     // CRTC R12:R13
    uint cursor_addr;    // CRTC R14:R15
    uint cursor_start;   // cursor start scanline
    uint cursor_end;     // cursor end scanline
    uint cursor_enabled; // 1 = cursor on
    uint max_scanline;   // CRTC R9: char height = max_scanline + 1
    uint h_displayed;    // CRTC R1: columns displayed
    uint v_displayed;    // CRTC R6: rows displayed
    uint h_total;        // CRTC R0: horizontal total (char clocks - 1)
    uint hsync_pos;      // CRTC R2: horizontal sync position
    uint hsync_width;    // CRTC R3 low nibble: hsync width in char clocks
    uint v_total;        // CRTC R4: vertical total (char rows - 1)
    uint vtotal_adj;     // CRTC R5: vertical total adjust (scanlines)
    uint vsync_pos;      // CRTC R7: vertical sync position
    uint _pad0, _pad1;
};

// --- Resources ---
Buffer<uint> vram : register(t0);          // 16KB VRAM (4096 uint32s)
Buffer<uint> font_buf : register(t1);      // 2048-byte 8x8 font ROM (512 uint32s)
Buffer<uint> palette : register(t2);       // 16 RGBA colors

Buffer<uint> scanline_buf : register(t3);  // 262 scanlines x 52 uint32s (12 header + 40 vram row)

// Stride per scanline in the scanline buffer (uint32s).
#define SL_STRIDE 52
#define SL_VRAM_OFFSET 12  // vram_row starts at uint32 index 12

RWTexture2D<float4> output_tex : register(u0);  // 912x262 full NTSC frame

// --- Mode bits ---
#define MODE_HIRES_TEXT  0x01
#define MODE_GRAPHICS    0x02
#define MODE_BW          0x04
#define MODE_ENABLE      0x08
#define MODE_HIRES_GFX   0x10
#define MODE_BLINK       0x20

// Read a byte from the global VRAM buffer (for font ROM fallback etc.)
uint vram_byte(uint addr) {
    uint word = vram[addr >> 2];
    return (word >> ((addr & 3) * 8)) & 0xFF;
}

// Read a byte from a scanline's captured VRAM row.
// `sl_base` is the scanline's base index in scanline_buf.
// `byte_idx` is the byte offset within the row (0..159).
uint sl_vram_byte(uint sl_base, uint byte_idx) {
    uint word_idx = sl_base + SL_VRAM_OFFSET + (byte_idx >> 2);
    uint word = scanline_buf[word_idx];
    return (word >> ((byte_idx & 3) * 8)) & 0xFF;
}

// Read a byte from font ROM
uint font_byte(uint addr) {
    uint word = font_buf[addr >> 2];
    return (word >> ((addr & 3) * 8)) & 0xFF;
}

// Unpack palette color to float4
float4 pal_color(uint idx) {
    uint rgba = palette[idx & 0xF];
    return float4(
        ((rgba >>  0) & 0xFF) / 255.0,
        ((rgba >>  8) & 0xFF) / 255.0,
        ((rgba >> 16) & 0xFF) / 255.0,
        1.0
    );
}

// Graphics palette lookup (6 palettes, 4 entries each)
// Encoded as constants to avoid another buffer.
static const uint gfx_pal[6][4] = {
    {0, 2, 4, 6},    {0, 10, 12, 14},
    {0, 3, 5, 7},    {0, 11, 13, 15},
    {0, 3, 4, 7},    {0, 11, 12, 15},
};

// =========================================================================
// CGA CRT beam model.
//
// Each thread IS a dot on the phosphor at position (px, py) within the
// full 912x262 NTSC frame.  The MC6845 CRTC counters determine what the
// beam produces at each dot:
//
//   Horizontal: char counter 0..R0.
//     0..R1-1           = active display (VRAM fetch)
//     R1..R2-1          = right overscan (border color)
//     R2..R2+hsync_w-1  = hsync (blanked, black)
//     R2+hsync_w..R0    = left overscan of next line (border color)
//
//   Vertical: row counter 0..R4, scanline counter 0..R9 within each row.
//     row 0..R6-1       = active display rows
//     row R6..R7-1      = bottom overscan
//     row R7            = vsync start (16 scanlines, blanked)
//     remaining         = top overscan (wraps visually)
//
// Character width: 8 dots (hires/80-col) or 16 dots (lores/40-col).
// Both produce 912 dots/line: 114*8 or 57*16.
// =========================================================================

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
    uint px = dtid.x;  // 0-911  dot position in full NTSC line
    uint py = dtid.y;  // 0-261  scanline in full NTSC frame
    if (px >= 912 || py >= 262) return;

    // Per-scanline register state (beam-racing support).
    // Programs that switch modes mid-frame also reprogram R9, R1, R6,
    // so ALL rendering-critical registers are captured per-scanline.
    uint sl_base = py * SL_STRIDE;  // 52 uint32s per scanline
    uint sl_mode          = scanline_buf[sl_base + 0];
    uint sl_color         = scanline_buf[sl_base + 1];
    uint sl_ma            = scanline_buf[sl_base + 2];  // MA: linear address
    uint sl_ra            = scanline_buf[sl_base + 3];  // RA: scanline within char row
    uint sl_vcc           = scanline_buf[sl_base + 4];  // VCC: character row counter
    uint sl_h_displayed   = scanline_buf[sl_base + 5];
    uint sl_v_displayed   = scanline_buf[sl_base + 6];
    uint sl_hsync_pos     = scanline_buf[sl_base + 7];
    uint sl_hsync_width   = scanline_buf[sl_base + 8];
    uint h_total          = scanline_buf[sl_base + 9];

    float4 border = pal_color(sl_color & 0xF);

    // Character width in dots -- fixed by dot clock divider.
    bool hires = (sl_mode & MODE_GRAPHICS) ? (sl_mode & MODE_HIRES_GFX) != 0
                                           : (sl_mode & MODE_HIRES_TEXT) != 0;
    uint char_w = hires ? 8 : 16;

    // 6845 counters from the per-scanline buffer.
    uint char_row = sl_vcc;
    uint scanline = sl_ra;

    // --- Horizontal beam position ---
    // The beam scans in this order after HSYNC end (px=0 in our buffer):
    //   left overscan:  (R0+1 - R2 - R3) char clocks  -> border
    //   active display: R1 char clocks (HCC 0..R1-1)   -> VRAM content
    //   right overscan: (R2 - R1) char clocks          -> border
    //   HSYNC:          R3 char clocks                  -> black
    //
    // All in character clock units, converted to dots via char_w.
    uint h_total_chars = (h_total > 0) ? h_total + 1 : 114;
    uint left_porch = h_total_chars - sl_hsync_pos - sl_hsync_width;
    uint left_porch_dots = left_porch * char_w;

    uint h_disp = (sl_h_displayed > 0) ? sl_h_displayed : (hires ? 80 : 40);
    uint active_dots = h_disp * char_w;
    uint right_porch = sl_hsync_pos - h_disp;
    uint right_porch_dots = right_porch * char_w;
    uint hsync_dots = sl_hsync_width * char_w;

    // Classify this pixel.
    uint region_px = px;
    float4 out_color;

    if (region_px < left_porch_dots) {
        // Left overscan (back porch) -- border color.
        output_tex[dtid.xy] = border;
        return;
    }
    region_px -= left_porch_dots;

    if (region_px < active_dots) {
        // Active display region.  region_px is the dot within active area.
        uint active_px = region_px;
        uint char_col = active_px / char_w;

        // Vertical display enable: VCC < R6.
        uint v_disp = (sl_v_displayed > 0) ? sl_v_displayed : 25;
        if (char_row >= v_disp || !(sl_mode & MODE_ENABLE)) {
            output_tex[dtid.xy] = border;
            return;
        }

        if (sl_mode & MODE_GRAPHICS) {
            if (sl_mode & MODE_HIRES_GFX) {
                uint byte_val = sl_vram_byte(sl_base, active_px / 8);
                uint lit = (byte_val >> (7 - (active_px & 7))) & 1;
                uint fg = sl_color & 0xF;
                if (fg == 0) fg = 15;
                out_color = pal_color(lit ? fg : 0);
            } else {
                uint src_x = active_px / 2;
                uint byte_val = sl_vram_byte(sl_base, src_x / 4);
                uint pixel = (byte_val >> (6 - (src_x & 3) * 2)) & 3;

                uint color_idx;
                if (pixel == 0) {
                    color_idx = sl_color & 0xF;
                } else {
                    uint base = (sl_color & 0x10) ? 8 : 0;
                    if (sl_mode & MODE_BW)
                        color_idx = gfx_pal[4 + (base ? 1 : 0)][pixel];
                    else if (sl_color & 0x20)
                        color_idx = gfx_pal[2 + (base ? 1 : 0)][pixel];
                    else
                        color_idx = gfx_pal[0 + (base ? 1 : 0)][pixel];
                }
                out_color = pal_color(color_idx);
            }
        } else {
            uint cell = (sl_ma + char_col) & 0x1FFF;
            uint ch   = sl_vram_byte(sl_base, char_col * 2);
            uint attr  = sl_vram_byte(sl_base, char_col * 2 + 1);

            uint fg = attr & 0x0F;
            uint bg = (attr >> 4) & 0x0F;

            if (sl_mode & MODE_BLINK) {
                bg &= 0x07;
                if ((attr & 0x80) && !attr_blink)
                    fg = bg;
            }

            uint font_sl = scanline < 8 ? scanline : (scanline & 7);
            uint glyph = font_byte(ch * 8 + font_sl);

            uint dot_in_cell = active_px % char_w;
            uint font_col = dot_in_cell * 8 / char_w;
            uint bit = (glyph >> (7 - font_col)) & 1;

            if (cursor_enabled && cursor_blink &&
                cell == cursor_addr &&
                scanline >= cursor_start && scanline <= cursor_end) {
                out_color = pal_color(fg);
            } else {
                out_color = pal_color(bit ? fg : bg);
            }
        }

        output_tex[dtid.xy] = out_color;
        return;
    }
    region_px -= active_dots;

    if (region_px < right_porch_dots) {
        // Right overscan (front porch) -- border color.
        output_tex[dtid.xy] = border;
        return;
    }

    // HSYNC -- blanked (black).
    output_tex[dtid.xy] = float4(0, 0, 0, 1);
}
)HLSL";

const ISA_Card* CgaRasterizer::card() const {
    return cga_card_;
}

Rasterizer::UVRect CgaRasterizer::output_uv_rect() const {
    // Crop to the active display area (where DE is active).
    // Vertically: starts at active_start_ (first VCC=0 after VSYNC).
    // Horizontally: starts after left overscan (back porch).
    const uint8_t* r = cga_card_->crtc_regs();
    bool hires = (cga_card_->mode_register() & ISA_CGA::MODE_HIRES_TEXT) ||
                 (cga_card_->mode_register() & ISA_CGA::MODE_HIRES_GFX);
    uint32_t char_w = hires ? 8 : 16;
    uint32_t h_total_chars = r[ISA_CGA::CRTC_HTOTAL] + 1;
    uint32_t hsync_pos = r[ISA_CGA::CRTC_HSYNC_POS];
    uint32_t hsync_width = r[ISA_CGA::CRTC_SYNC_WIDTH] & 0x0F;
    uint32_t left_porch_dots = (h_total_chars - hsync_pos - hsync_width) * char_w;

    uint32_t top = cga_card_->active_start_scanline();
    if (top >= (uint32_t)OUT_H) top = 0;

    float u0 = float(left_porch_dots) / float(OUT_W);
    float v0 = float(top) / float(OUT_H);
    float u1 = float(left_porch_dots + VIEW_W) / float(OUT_W);
    float v1 = float(top + VIEW_H) / float(OUT_H);
    if (u1 > 1.0f) u1 = 1.0f;
    if (v1 > 1.0f) v1 = 1.0f;
    return { u0, v0, u1, v1 };
}

bool CgaRasterizer::init(const RenderContext& rc) {
    auto* device = rc.device;
    // Compile compute shader
    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3DCompile(CGA_CS_HLSL, sizeof(CGA_CS_HLSL), "cga_cs",
                            nullptr, nullptr, "CSMain", "cs_5_0",
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                            &blob, &err);
    if (FAILED(hr)) {
        if (err) spdlog::error("[CGA] shader compile: {}", (char*)err->GetBufferPointer());
        return false;
    }
    hr = device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(),
                                     nullptr, &cs_);
    if (FAILED(hr)) { spdlog::error("[CGA] CreateComputeShader failed"); return false; }

    // VRAM buffer (16KB, Buffer<uint>)
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = ISA_CGA::FB_SIZE;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, &vram_buf_);

        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32_UINT;
        srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv.Buffer.NumElements = ISA_CGA::FB_SIZE / 4;
        HRESULT hr2 = device->CreateShaderResourceView(vram_buf_.Get(), &srv, &vram_srv_);
        if (FAILED(hr2)) spdlog::error("[CGA] VRAM SRV failed: 0x{:08X}", (unsigned)hr2);
    }

    // Font ROM buffer (2048 bytes, Buffer<uint>) -- uploaded on first render
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = 2048;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        device->CreateBuffer(&bd, nullptr, &font_buf_);

        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32_UINT;
        srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv.Buffer.NumElements = 2048 / 4;
        HRESULT hr2 = device->CreateShaderResourceView(font_buf_.Get(), &srv, &font_srv_);
        if (FAILED(hr2)) spdlog::error("[CGA] Font SRV failed: 0x{:08X}", (unsigned)hr2);
    }

    // Constant buffer
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = sizeof(ISA_CGA::GpuConstants);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, &cb_);
    }

    // Palette buffer (16 RGBA uint32s)
    {
        uint32_t pal[16];
        for (int i = 0; i < 16; ++i) {
            auto& c = ISA_CGA::PALETTE[i];
            pal[i] = c.r | (c.g << 8) | (c.b << 16) | (0xFF << 24);
        }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = 16 * sizeof(uint32_t);
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init = { pal, 0, 0 };
        device->CreateBuffer(&bd, &init, &palette_buf_);

        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32_UINT;
        srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv.Buffer.NumElements = 16;
        device->CreateShaderResourceView(palette_buf_.Get(), &srv, &palette_srv_);
    }

    // Per-scanline register buffer (262 scanlines x 12 uint32s = 48 bytes each)
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = ISA_CGA::FRAME_LINES * sizeof(ISA_CGA::ScanlineRegs);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer(&bd, nullptr, &scanline_buf_);

        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.Format = DXGI_FORMAT_R32_UINT;
        srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srv.Buffer.NumElements = ISA_CGA::FRAME_LINES * (sizeof(ISA_CGA::ScanlineRegs) / 4);
        HRESULT hr2 = device->CreateShaderResourceView(scanline_buf_.Get(), &srv, &scanline_srv_);
        if (FAILED(hr2)) spdlog::error("[CGA] Scanline SRV failed: 0x{:08X}", (unsigned)hr2);
    }

    // Output texture (912x262 RGBA8)
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

    spdlog::info("[CGA] GPU rasterizer initialized ({}x{}, viewport {}x{})",
                 OUT_W, OUT_H, VIEW_W, VIEW_H);
    return true;
}

void CgaRasterizer::render(const RenderContext& rc) {
    if (!cga_card_ || !cs_) return;
    auto* ctx = rc.d3d_ctx;
    const auto* card = cga_card_;

    // Upload font ROM (once, on first render)
    static bool font_uploaded = false;
    if (!font_uploaded && font_buf_) {
        ctx->UpdateSubresource(font_buf_.Get(), 0, nullptr, card->font_rom(), ISA_CGA::FONT_SIZE, 0);
        font_uploaded = true;
    }

    // Upload VRAM
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(vram_buf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, card->vram(), ISA_CGA::FB_SIZE);
        ctx->Unmap(vram_buf_.Get(), 0);
    }

    // Upload constants
    {
        ISA_CGA::GpuConstants cb;
        card->fill_gpu_constants(cb);

        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(cb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &cb, sizeof(cb));
        ctx->Unmap(cb_.Get(), 0);
    }

    // Upload per-scanline register snapshots (beam-racing)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        ctx->Map(scanline_buf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, card->scanline_regs(),
               ISA_CGA::FRAME_LINES * sizeof(ISA_CGA::ScanlineRegs));
        ctx->Unmap(scanline_buf_.Get(), 0);
    }

    // Dispatch compute shader
    ctx->CSSetShader(cs_.Get(), nullptr, 0);
    ctx->CSSetConstantBuffers(0, 1, cb_.GetAddressOf());
    ID3D11ShaderResourceView* srvs[] = { vram_srv_.Get(), font_srv_.Get(), palette_srv_.Get(), scanline_srv_.Get() };
    ctx->CSSetShaderResources(0, 4, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, out_uav_.GetAddressOf(), nullptr);

    // 912x262 / (16,16) = (57, 17) thread groups
    ctx->Dispatch((OUT_W + 15) / 16, (OUT_H + 15) / 16, 1);

    // Unbind
    ID3D11UnorderedAccessView* null_uav = nullptr;
    ctx->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
    ID3D11ShaderResourceView* null_srvs[4] = {};
    ctx->CSSetShaderResources(0, 4, null_srvs);

    // output_srv() is now valid -- renderer blits it to the swap chain.
}

} // namespace bench
