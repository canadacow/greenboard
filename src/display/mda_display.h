#pragma once
// MdaRasterizer -- 80x25 MDA text rasterization via Direct2D/DirectWrite.

#include "display/rasterizer.h"
#include <cstdint>
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <wrl/client.h>

namespace bench {

class ISA_MDA;

class MdaRasterizer final : public Rasterizer {
public:
    MdaRasterizer(const ISA_MDA* card, const uint8_t* vram)
        : mda_card_(card), vram_(vram) {}

    bool init(const RenderContext& rc) override;
    void render(const RenderContext& rc) override;
    const ISA_Card* card() const override;
    bool uses_d2d() const override { return true; }

    // d2dTarget_ wraps a swap chain back buffer; drop it (and the context's
    // reference to it) so ResizeBuffers can succeed. render() recreates it.
    void release_backbuffer_refs() override {
        if (d2dCtx_) d2dCtx_->SetTarget(nullptr);
        d2dTarget_.Reset();
        d2d_target_res_.Reset();
    }

private:
    const ISA_MDA* mda_card_;
    const uint8_t* vram_;

    // D2D owned by MDA (not shared with renderer)
    Microsoft::WRL::ComPtr<ID2D1Factory1> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1Device> d2dDevice_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> d2dCtx_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> d2dTarget_;
    // Back buffer d2dTarget_ currently wraps -- flip-model rotates buffers,
    // so this detects when the target must be rebuilt.
    Microsoft::WRL::ComPtr<ID3D11Resource> d2d_target_res_;
    Microsoft::WRL::ComPtr<IDWriteFactory5> dwriteFactory_;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> greenBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brightGreenBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> blackBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> underlineBrush_;

    LARGE_INTEGER qpc_freq_ = {};
    LARGE_INTEGER qpc_start_ = {};
    static constexpr double MDA_FIELD_HZ = 18432000.0 / (882.0 * 370.0);

    float cellW_ = 0;
    float cellH_ = 0;
};

} // namespace bench
