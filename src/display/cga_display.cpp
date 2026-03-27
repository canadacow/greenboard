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

// --- Constant buffer: CGA register state (no arrays -- avoid HLSL 16-byte alignment) ---
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
    uint _pad0, _pad1, _pad2;
};

// --- Resources ---
Buffer<uint> vram : register(t0);          // 16KB VRAM (4096 uint32s)
Buffer<uint> font_buf : register(t1);      // 2048-byte 8x8 font ROM (512 uint32s)
Buffer<uint> palette : register(t2);       // 16 RGBA colors

RWTexture2D<float4> output_tex : register(u0);  // 640x200 RGBA output

// --- Mode bits ---
#define MODE_HIRES_TEXT  0x01
#define MODE_GRAPHICS    0x02
#define MODE_BW          0x04
#define MODE_ENABLE      0x08
#define MODE_HIRES_GFX   0x10
#define MODE_BLINK       0x20

// Read a byte from VRAM
uint vram_byte(uint addr) {
    uint word = vram[addr >> 2];
    return (word >> ((addr & 3) * 8)) & 0xFF;
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
// CGA rendering -- follows DOSBox-X vga_draw.cpp logic.
//
// Output is always 640x200 RGBA.  Text modes render 8 pixels per character
// (40-col is pixel-doubled).  Graphics modes render at native resolution
// (320x200 pixel-doubled, 640x200 native).
//
// CRTC registers drive character height (R9), displayed columns (R1),
// and displayed rows (R6).  This handles standard modes AND tweaked modes
// like 160x100x16 (R9=1, R1=80, R6=100).
// =========================================================================

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
    uint px = dtid.x;  // 0-639
    uint py = dtid.y;  // 0-199
    if (px >= 640 || py >= 200) return;

    float4 overscan = pal_color(color & 0xF);

    // Display disabled: overscan
    if (!(mode & MODE_ENABLE)) {
        output_tex[dtid.xy] = overscan;
        return;
    }

    float4 out_color = float4(0, 0, 0, 1);

    if (mode & MODE_GRAPHICS) {
        // =============================================================
        // Graphics modes (interleaved scanlines: even at +0, odd at +0x2000)
        // =============================================================
        uint row_addr = (py / 2) * 80 + (py & 1) * 0x2000;

        if (mode & MODE_HIRES_GFX) {
            // 640x200, 1bpp.  8 pixels per byte.
            uint byte_val = vram_byte(row_addr + px / 8);
            uint lit = (byte_val >> (7 - (px & 7))) & 1;
            uint fg = color & 0xF;
            if (fg == 0) fg = 15;  // default white if overscan black
            out_color = pal_color(lit ? fg : 0);
        } else {
            // 320x200, 2bpp.  4 pixels per byte, doubled to 640.
            uint src_x = px / 2;
            uint byte_val = vram_byte(row_addr + src_x / 4);
            uint pixel = (byte_val >> (6 - (src_x & 3) * 2)) & 3;

            uint color_idx;
            if (pixel == 0) {
                color_idx = color & 0xF;
            } else {
                // DOSBox vga_other.cpp write_cga_color_select():
                // BW bit selects alternate palette; otherwise palette/bright bits.
                uint base = (color & 0x10) ? 8 : 0;  // intensity
                if (mode & MODE_BW)
                    color_idx = gfx_pal[4 + (base ? 1 : 0)][pixel];
                else if (color & 0x20)
                    color_idx = gfx_pal[2 + (base ? 1 : 0)][pixel];
                else
                    color_idx = gfx_pal[0 + (base ? 1 : 0)][pixel];
            }
            out_color = pal_color(color_idx);
        }
    } else {
        // =============================================================
        // Text modes (40x25, 80x25, or CRTC-tweaked like 80x100)
        //
        // DOSBox: each character is always 8 output pixels wide.
        //   40-col mode (MODE_HIRES_TEXT=0): pixel-doubled -> 16px/char
        //   80-col mode (MODE_HIRES_TEXT=1): native -> 8px/char
        //
        // Character height from CRTC R9 (max_scanline + 1).
        // Column count from CRTC R1 (h_displayed), default 80 or 40.
        // Row count from CRTC R6 (v_displayed), default 200/char_h.
        // =============================================================
        uint char_h = (max_scanline & 0x1F) + 1;
        if (char_h == 0 || char_h > 32) char_h = 8;

        // Columns from CRTC R1. Default from mode bit if R1 not yet programmed.
        uint cols = (h_displayed > 0 && h_displayed <= 160) ? h_displayed
                  : ((mode & MODE_HIRES_TEXT) ? 80 : 40);
        uint char_w = 640 / cols;
        if (char_w == 0) char_w = 8;
        uint rows = (v_displayed > 0 && v_displayed <= 128) ? v_displayed : (200 / char_h);

        uint col = px / char_w;
        uint row = py / char_h;
        uint scanline = py % char_h;

        if (col >= cols || row >= rows) {
            out_color = overscan;
        } else {
            // VRAM address: (start_addr + row * cols + col) * 2
            uint cell = start_addr + row * cols + col;
            uint addr = (cell * 2) & 0x3FFF;
            uint ch   = vram_byte(addr);
            uint attr  = vram_byte(addr + 1);

            // Attribute decode (DOSBox vga_draw.cpp line 2069-2070):
            //   fg = attr[3:0]      (16 foreground colors)
            //   bg = attr[6:4]      (8 background colors)
            //   blink = attr[7]
            uint fg = attr & 0x0F;
            uint bg = (attr >> 4) & 0x0F;  // full 4 bits initially

            if (mode & MODE_BLINK) {
                // Blink mode: bg is 3 bits (0-7), bit 7 controls blink.
                bg &= 0x07;
                // DOSBox FontMask: when blink bit set and blink phase off,
                // font mask = 0 -> all pixels show background.
                if ((attr & 0x80) && !attr_blink)
                    fg = bg;
            }
            // else: intensity mode -- bg keeps all 4 bits (0-15).

            // Font lookup.  ROM is 8 bytes per character.
            // Clamp scanline to 0-7 for font ROM access.
            uint font_sl = scanline < 8 ? scanline : (scanline & 7);
            uint glyph = font_byte(ch * 8 + font_sl);

            // Pixel within character cell -> font column (0-7).
            // Maps char_w output pixels to 8 font columns.
            uint font_col = (px % char_w) * 8 / char_w;
            uint bit = (glyph >> (7 - font_col)) & 1;

            // Cursor overlay (DOSBox vga_draw.cpp line 2078-2086).
            if (cursor_enabled && cursor_blink &&
                cell == cursor_addr &&
                scanline >= cursor_start && scanline <= cursor_end) {
                // Cursor: force foreground color across full cell width.
                out_color = pal_color(fg);
            } else {
                out_color = pal_color(bit ? fg : bg);
            }
        }
    }

    output_tex[dtid.xy] = out_color;
}
)HLSL";

const ISA_Card* CgaRasterizer::card() const {
    return cga_card_;
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

    // Output texture (640x200 RGBA8)
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

    spdlog::info("[CGA] GPU rasterizer initialized ({}x{})", OUT_W, OUT_H);
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

    // Dispatch compute shader
    ctx->CSSetShader(cs_.Get(), nullptr, 0);
    ctx->CSSetConstantBuffers(0, 1, cb_.GetAddressOf());
    ID3D11ShaderResourceView* srvs[] = { vram_srv_.Get(), font_srv_.Get(), palette_srv_.Get() };
    ctx->CSSetShaderResources(0, 3, srvs);
    ctx->CSSetUnorderedAccessViews(0, 1, out_uav_.GetAddressOf(), nullptr);

    // 640x200 / (16,16) = (40, 13) thread groups
    ctx->Dispatch((OUT_W + 15) / 16, (OUT_H + 15) / 16, 1);

    // Unbind
    ID3D11UnorderedAccessView* null_uav = nullptr;
    ctx->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
    ID3D11ShaderResourceView* null_srvs[3] = {};
    ctx->CSSetShaderResources(0, 3, null_srvs);

    // output_srv() is now valid -- renderer blits it to the swap chain.
}

} // namespace bench
