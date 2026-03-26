#pragma once
// Rasterizer -- abstract base for display card rendering.
//
// The Renderer owns one Rasterizer and calls it each frame.
// MdaRasterizer and CgaRasterizer both implement this interface.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <dwrite_3.h>

namespace bench {

class ISA_Card;

// GPU context bundle passed to rasterizers each frame.
struct RenderContext {
    ID3D11Device*        device;
    ID3D11DeviceContext*  d3d_ctx;
    ID2D1DeviceContext*   d2d_ctx;
    IDWriteFactory5*      dwrite;
    int winW, winH;
    float cellW, cellH;  // character cell size (for text modes)
};

class Rasterizer {
public:
    virtual ~Rasterizer() = default;

    // One-time GPU resource creation. Called after device init.
    virtual bool init(const RenderContext& rc) = 0;

    // Render one frame. Called between BeginDraw/EndDraw (D2D) or with
    // the D3D context active. The rasterizer draws into the current
    // render target (D2D) or dispatches compute and blits.
    virtual void render(const RenderContext& rc) = 0;

    // The ISA card this rasterizer is attached to.
    virtual const ISA_Card* card() const = 0;

    // Output texture SRV for D3D11 blit (nullptr if renders directly via D2D).
    virtual ID3D11ShaderResourceView* output_srv() const { return nullptr; }

    // Whether this rasterizer uses D2D (MDA) or D3D11 texture output (CGA).
    virtual bool uses_d2d() const { return false; }
};

} // namespace bench
