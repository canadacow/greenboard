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

    // Whether this rasterizer renders directly to the back buffer via D2D.
    virtual bool uses_d2d() const { return false; }
};

} // namespace bench
