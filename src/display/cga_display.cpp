// CgaRasterizer -- GPU-accelerated CGA display.

#include "display/cga_display.h"
#include "isa/isa_card.h"
#include "isa/isa_cga.h"
#include <d3dcompiler.h>
#include <spdlog/spdlog.h>

using Microsoft::WRL::ComPtr;

namespace bench {

// Embedded HLSL source compiled at runtime.
static const char CGA_CS_HLSL[] = R"HLSL(

// --- Constant buffer: CGA register state ---
cbuffer CGA_CB : register(b0) {
    uint mode;           // 0x3D8
    uint color;          // 0x3D9
    uint crtc[18];       // MC6845 registers
    uint blink_on;       // 1 = blink visible
    uint composite;      // 1 = composite decode (TODO)
    uint start_addr;     // CRTC R12:R13
    uint cursor_addr;    // CRTC R14:R15
    uint cursor_start;   // cursor start scanline
    uint cursor_end;     // cursor end scanline
    uint cursor_enabled; // 1 = cursor on
    uint _pad0, _pad1;
};

// --- Resources ---
ByteAddressBuffer vram : register(t0);     // 16KB VRAM
ByteAddressBuffer font : register(t1);     // 2048-byte 8x8 font ROM
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
    uint word = vram.Load((addr & 0x3FFC));  // align to 4 bytes
    return (word >> ((addr & 3) * 8)) & 0xFF;
}

// Read a byte from font ROM
uint font_byte(uint addr) {
    uint word = font.Load((addr & 0x7FC));
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

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID) {
    int px = dtid.x;  // 0-639
    int py = dtid.y;  // 0-199
    if (px >= 640 || py >= 200) return;

    // Display disabled: overscan color
    if (!(mode & MODE_ENABLE)) {
        output_tex[dtid.xy] = pal_color(color & 0xF);
        return;
    }

    float4 out_color = float4(0, 0, 0, 1);

    if (mode & MODE_GRAPHICS) {
        // --- Graphics modes ---
        if (mode & MODE_HIRES_GFX) {
            // 640x200, 1 bit per pixel
            uint row_addr = (py / 2) * 80 + (py & 1) * 0x2000;
            uint byte_x = px / 8;
            uint bit = 7 - (px & 7);
            uint byte_val = vram_byte(row_addr + byte_x);
            uint lit = (byte_val >> bit) & 1;
            uint fg = color & 0xF;
            if (fg == 0) fg = 15;
            out_color = pal_color(lit ? fg : 0);
        } else {
            // 320x200, 2 bits per pixel (pixel-doubled to 640)
            int src_x = px / 2;  // 0-319
            uint row_addr = (py / 2) * 80 + (py & 1) * 0x2000;
            uint byte_x = src_x / 4;
            uint shift = 6 - (src_x & 3) * 2;
            uint byte_val = vram_byte(row_addr + byte_x);
            uint pixel = (byte_val >> shift) & 0x3;

            // Palette selection
            uint pal_idx = 0;
            if (color & 0x20) pal_idx += 2;  // CC_PALETTE
            if (color & 0x10) pal_idx += 1;  // CC_BRIGHT

            uint color_idx;
            if (pixel == 0)
                color_idx = color & 0xF;  // background/overscan
            else
                color_idx = gfx_pal[pal_idx][pixel];

            out_color = pal_color(color_idx);
        }
    } else {
        // --- Text modes ---
        uint cols = (mode & MODE_HIRES_TEXT) ? 80 : 40;
        uint char_w = (mode & MODE_HIRES_TEXT) ? 8 : 16;  // pixel width per char

        uint col = px / char_w;
        uint row = py / 8;
        uint scanline = py & 7;

        if (col >= cols || row >= 25) {
            out_color = pal_color(color & 0xF);  // overscan
        } else {
            uint addr = (start_addr + row * cols + col) * 2;
            uint ch = vram_byte(addr & 0x3FFF);
            uint attr = vram_byte((addr + 1) & 0x3FFF);

            uint fg = attr & 0x0F;
            uint bg = (attr >> 4) & 0x07;
            bool blink_bit = (attr & 0x80) != 0;

            // Blink vs intensity
            if ((mode & MODE_BLINK) && blink_bit && !blink_on)
                fg = bg;  // hide character
            if (!(mode & MODE_BLINK) && blink_bit)
                bg |= 0x08;  // high-intensity background

            // Font lookup
            uint glyph_row = font_byte(ch * 8 + scanline);
            uint local_px = (mode & MODE_HIRES_TEXT) ? (px & 7) : ((px / 2) & 7);
            uint bit = (glyph_row >> (7 - local_px)) & 1;

            // Cursor overlay
            bool is_cursor = (cursor_enabled != 0) && (blink_on != 0) &&
                ((start_addr + row * cols + col) == cursor_addr) &&
                (scanline >= cursor_start) && (scanline <= cursor_end);

            if (is_cursor || bit)
                out_color = pal_color(fg);
            else
                out_color = pal_color(bg);
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

    // VRAM buffer (16KB, ByteAddressBuffer)
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = ISA_CGA::FB_SIZE;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        device->CreateBuffer(&bd, nullptr, &vram_buf_);

        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        srv.BufferEx.NumElements = ISA_CGA::FB_SIZE / 4;
        device->CreateShaderResourceView(vram_buf_.Get(), &srv, &vram_srv_);
    }

    // Font ROM buffer (2048 bytes, ByteAddressBuffer)
    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = 2048;
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
        D3D11_SUBRESOURCE_DATA init = { ISA_CGA::FONT_8X8, 0, 0 };
        device->CreateBuffer(&bd, &init, &font_buf_);

        D3D11_SHADER_RESOURCE_VIEW_DESC srv = {};
        srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
        srv.BufferEx.NumElements = 2048 / 4;
        device->CreateShaderResourceView(font_buf_.Get(), &srv, &font_srv_);
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

}

} // namespace bench
