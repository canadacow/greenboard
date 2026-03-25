#pragma once
// BoardView -- zoomable SDF renderer for 5150 motherboard PCB traces.
//
// Loads board geometry from JSON (output of scripts/brd_to_json.py),
// uploads it as a DX11 structured buffer, and renders via a pixel shader
// that evaluates signed-distance functions for every visible segment.
//
// Integrates into the existing ImGui/DX11 window as a togglable panel.

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace bench {

using namespace Microsoft::WRL;

class BoardView {
public:
    // Load geometry from JSON file and create GPU resources.
    bool init(ID3D11Device* device, const char* json_path);

    // Render the board view as an ImGui window.
    // Call between ImGui::NewFrame() and ImGui::Render().
    void imgui_window(ID3D11DeviceContext* ctx);

    bool is_open() const { return open_; }
    void toggle() { open_ = !open_; }

private:
    // GPU segment -- matches HLSL StructuredBuffer layout (32 bytes).
    struct GpuSegment {
        float x1, y1, x2, y2;
        float half_w;
        uint32_t rgba;
        uint32_t net;
        uint32_t flags;  // 0=trace_top, 1=trace_bot, 2=via, 3=pad, 4=outline
    };
    static_assert(sizeof(GpuSegment) == 32);

    // View constant buffer -- matches HLSL cbuffer.
    struct alignas(16) ViewCB {
        float view_min_x, view_min_y;
        float view_size_x, view_size_y;
        float screen_w, screen_h;
        uint32_t seg_count;
        uint32_t highlight_net;
        float bg_r, bg_g, bg_b, bg_a;
        uint32_t layer_mask;   // bitmask: bit 0 = top, bit 1 = bottom, bit 2 = vias, etc.
        float pad[3];
    };
    static_assert(sizeof(ViewCB) % 16 == 0);

    bool load_json(const char* path);
    bool create_gpu_resources(ID3D11Device* device);
    void render_to_texture(ID3D11DeviceContext* ctx, int w, int h);
    uint32_t hit_test(float board_x, float board_y) const;

    // Geometry
    std::vector<GpuSegment> segments_;
    float bounds_[4] = {};  // x_min, y_min, x_max, y_max (mils)
    std::unordered_map<uint32_t, std::string> net_names_;

    // Component labels (ref designator + center position)
    struct CompLabel {
        std::string ref;
        std::string value;
        float x, y;
    };
    std::vector<CompLabel> labels_;

    // View state
    float pan_x_ = 0, pan_y_ = 0;  // board-space center
    float zoom_ = 1.0f;             // pixels per mil
    bool open_ = false;
    uint32_t highlight_net_ = 0;
    uint32_t layer_mask_ = 0x1F;    // all layers visible

    // DX11 resources
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11VertexShader> vs_;
    ComPtr<ID3D11PixelShader> ps_;
    ComPtr<ID3D11Buffer> cb_;
    ComPtr<ID3D11Buffer> seg_buf_;
    ComPtr<ID3D11ShaderResourceView> seg_srv_;

    // Offscreen render target
    ComPtr<ID3D11Texture2D> rt_tex_;
    ComPtr<ID3D11RenderTargetView> rt_rtv_;
    ComPtr<ID3D11ShaderResourceView> rt_srv_;
    int rt_w_ = 0, rt_h_ = 0;

    void ensure_rt(ID3D11DeviceContext* ctx, int w, int h);
};

} // namespace bench
