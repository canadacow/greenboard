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

    // Output texture dimensions
    static constexpr int OUT_W = 640;
    static constexpr int OUT_H = 200;

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
    Microsoft::WRL::ComPtr<ID3D11Texture2D> out_tex_;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> out_uav_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> out_srv_;
};

} // namespace bench
