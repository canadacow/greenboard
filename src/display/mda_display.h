#pragma once
// MdaRasterizer -- 80x25 MDA text rasterization via Direct2D/DirectWrite.
//
// Handles CP437 glyph rendering, MDA attribute decoding (normal, reverse,
// underline, blink, intensity), and MC6845 hardware cursor emulation.
// Completely separated from the windowing/ImGui layer (Renderer).

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <wrl/client.h>

namespace bench {

class ISA_MDA;

class MdaRasterizer {
public:
    // Initialize D2D brushes, DirectWrite text format (MDA font), and QPC timing.
    // cellW/cellH are the pixel dimensions of a single character cell.
    bool init(ID2D1DeviceContext* d2dCtx, IDWriteFactory5* dwriteFactory,
              float cellW, float cellH);

    // Set the MDA card pointer (for CRTC cursor registers).
    void set_mda_card(const ISA_MDA* mda_card) { mda_card_ = mda_card; }

    // Render the 80x25 MDA framebuffer using D2D.
    // Caller must have set the D2D render target before calling.
    void render(ID2D1DeviceContext* d2dCtx, const uint8_t* vram);

private:
    Microsoft::WRL::ComPtr<IDWriteTextFormat> textFormat_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> greenBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brightGreenBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> blackBrush_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> underlineBrush_;

    LARGE_INTEGER qpc_freq_ = {};
    LARGE_INTEGER qpc_start_ = {};
    static constexpr double MDA_FIELD_HZ = 18432000.0 / (882.0 * 370.0);  // ~56.5 Hz

    float cellW_ = 0;
    float cellH_ = 0;

    const ISA_MDA* mda_card_ = nullptr;
};

} // namespace bench
