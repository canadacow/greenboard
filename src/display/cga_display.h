#pragma once
// CgaRasterizer -- GPU-accelerated CGA display via D3D11 compute shader.

#include "display/rasterizer.h"
#include <cstdint>
#include <wrl/client.h>

namespace bench {

class ISA_CGA;

class CgaRasterizer final : public Rasterizer {
public:
    explicit CgaRasterizer(const ISA_CGA* card) : cga_card_(card) {}

    bool init(const RenderContext& rc) override;
    void render(const RenderContext& rc) override;
    const ISA_Card* card() const override;
    ID3D11ShaderResourceView* output_srv() const override { return out_srv_.Get(); }

    // Crop: 640x200 visible portion from the 912x262 full frame.
    // The buffer starts at VSYNC end (monitor retrace).  Active display
    // (VCC=0) begins after top overscan.  Compute the offset from
    // CRTC registers: scanlines from VSYNC end to VCC=0.
    UVRect output_uv_rect() const override;

    // Output texture: full NTSC frame in dot resolution.
    // 912 dots/line (114 char clocks * 8 dots), 262 scanlines/frame.
    // The renderer crops a 640x200 visible portion from this.
    static constexpr int OUT_W = 912;
    static constexpr int OUT_H = 262;

    // Visible portion cropped for display (standard CGA active area).
    static constexpr int VIEW_W = 640;
    static constexpr int VIEW_H = 200;

private:
    const ISA_CGA* cga_card_;

    Microsoft::WRL::ComPtr<ID3D11ComputeShader> cs_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vram_buf_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vram_srv_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> font_buf_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> font_srv_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> palette_buf_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> palette_srv_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> scanline_buf_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> scanline_srv_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> out_tex_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> out_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> out_srv_;
};

} // namespace bench
