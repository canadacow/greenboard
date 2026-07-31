#pragma once
// Rasterizer -- abstract base for display card rendering.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>

namespace bench {

class ISA_Card;

// GPU context bundle passed to rasterizers each frame.
struct RenderContext {
    ID3D11Device*        device;
    ID3D11DeviceContext*  d3d_ctx;
    int winW, winH;
    float cellW, cellH;
};

class Rasterizer {
public:
    virtual ~Rasterizer() = default;

    // One-time GPU resource creation.
    virtual bool init(const RenderContext& rc) = 0;

    // Render one frame.
    virtual void render(const RenderContext& rc) = 0;

    // The ISA card this rasterizer is attached to.
    virtual const ISA_Card* card() const = 0;

    // Output texture SRV for D3D11 blit (nullptr if renders via D2D).
    virtual ID3D11ShaderResourceView* output_srv() const { return nullptr; }

    // Raw color-index texture (R8_UINT, one 4-bit RGBI index per dot).
    // Used by the composite monitor filter, which needs the digital
    // RGBI stream rather than the rendered RGB. nullptr if unsupported.
    virtual ID3D11ShaderResourceView* index_srv() const { return nullptr; }

    // Source UV rect within the output texture for blitting.
    // Default: full texture.  Override to crop (e.g. 640x200 from 912x262).
    struct UVRect { float u0, v0, u1, v1; };
    virtual UVRect output_uv_rect() const { return {0, 0, 1, 1}; }

    // Dot-resolution canvas size of output_srv(). Sizes the renderer's
    // CRT-scaler/bezel intermediate targets. 0 = not texture-based.
    virtual int out_width() const { return 0; }
    virtual int out_height() const { return 0; }

    // Canvas subrect the monitor actually paints onto the tube: border
    // included, hsync/retrace and unused canvas excluded. The bezel
    // monitor maps this window onto the tube face.
    virtual UVRect painted_rect() const { return {0, 0, 1, 1}; }

    // Whether this rasterizer renders directly to the back buffer via D2D.
    virtual bool uses_d2d() const { return false; }

    // Drop any views/targets derived from the swap chain back buffers.
    // Must be called BEFORE ResizeBuffers -- the swap chain cannot be resized
    // while anything still references its buffers.
    virtual void release_backbuffer_refs() {}
};

} // namespace bench
