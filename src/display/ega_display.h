#pragma once
// EgaRasterizer -- GPU-accelerated EGA display via D3D11 compute shader.

#include "display/rasterizer.h"
#include <cstdint>
#include <wrl/client.h>

namespace bench {

class ISA_EGA;

class EgaRasterizer final : public Rasterizer {
public:
    explicit EgaRasterizer(const ISA_EGA* card) : ega_card_(card) {}

    bool init(const RenderContext& rc) override;
    void render(const RenderContext& rc) override;
    const ISA_Card* card() const override;
    ID3D11ShaderResourceView* output_srv() const override { return out_srv_.Get(); }
    int out_width() const override { return OUT_W; }
    int out_height() const override { return OUT_H; }

    // Crop the active display area out of the full scan canvas.
    // Horizontal: the canvas is anchored at the HSYNC leading edge, so
    // active video starts after hsync + back porch = (R0+2 - R4) chars.
    // Vertical: starts at active_start (first frame line after VSYNC).
    UVRect output_uv_rect() const override;

    // Output canvas in dot resolution. 912 dots covers the widest
    // standard line (CGA-compatible 200-line modes: 114 chars x 8 dots);
    // 350-line modes use 744 (93 x 8). 512 lines covers the EGA's 9-bit
    // vertical counter range as used by the standard modes (364 lines
    // at 60 Hz in 350-line modes, 262 in 200-line modes).
    static constexpr int OUT_W = 912;
    static constexpr int OUT_H = 512;

    // Nominal visible portion (640x350 in the hi-res EGA modes). The
    // actual crop is computed from live CRTC values in output_uv_rect().
    static constexpr int VIEW_W = 640;
    static constexpr int VIEW_H = 350;

private:
    const ISA_EGA* ega_card_;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> cs_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vram_buf_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vram_srv_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> scanline_buf_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scanline_srv_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> out_tex_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> out_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> out_srv_;
};

} // namespace bench
