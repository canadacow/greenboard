// MDA Display — DX11 + D2D + ImGui overlay.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dxgi.lib")

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include "display/mda_display.h"
#include "display/board_view.h"
#include "core/scheduler.h"
#include "ic/ic_8088.h"
#include "isa/isa_mda.h"
#include "debug/memory_view.h"
#include "test/test_keyboard.h"
#include "isa/isa_fdc.h"
#include <nfd.h>
#include <fstream>
#include "ic/ic_8237a.h"
#include <Zydis/Zydis.h>
#include <spdlog/spdlog.h>
#include <cinttypes>
#include <cmath>
#include <chrono>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using Microsoft::WRL::ComPtr;

namespace bench {

static constexpr int MDA_COLS = 80;
static constexpr int MDA_ROWS = 25;

// ========================================================================
// CP437 → Unicode
// ========================================================================

static const wchar_t cp437_low[32] = {
    L' ',      L'\u263A', L'\u263B', L'\u2665', L'\u2666', L'\u2663', L'\u2660', L'\u2022',
    L'\u25D8', L'\u25CB', L'\u25D9', L'\u2642', L'\u2640', L'\u266A', L'\u266B', L'\u263C',
    L'\u25BA', L'\u25C4', L'\u2195', L'\u203C', L'\u00B6', L'\u00A7', L'\u25AC', L'\u21A8',
    L'\u2191', L'\u2193', L'\u2192', L'\u2190', L'\u221F', L'\u2194', L'\u25B2', L'\u25BC',
};
static const wchar_t cp437_high[128] = {
    L'\u00C7', L'\u00FC', L'\u00E9', L'\u00E2', L'\u00E4', L'\u00E0', L'\u00E5', L'\u00E7',
    L'\u00EA', L'\u00EB', L'\u00E8', L'\u00EF', L'\u00EE', L'\u00EC', L'\u00C4', L'\u00C5',
    L'\u00C9', L'\u00E6', L'\u00C6', L'\u00F4', L'\u00F6', L'\u00F2', L'\u00FB', L'\u00F9',
    L'\u00FF', L'\u00D6', L'\u00DC', L'\u00A2', L'\u00A3', L'\u00A5', L'\u20A7', L'\u0192',
    L'\u00E1', L'\u00ED', L'\u00F3', L'\u00FA', L'\u00F1', L'\u00D1', L'\u00AA', L'\u00BA',
    L'\u00BF', L'\u2310', L'\u00AC', L'\u00BD', L'\u00BC', L'\u00A1', L'\u00AB', L'\u00BB',
    L'\u2591', L'\u2592', L'\u2593', L'\u2502', L'\u2524', L'\u2561', L'\u2562', L'\u2556',
    L'\u2555', L'\u2563', L'\u2551', L'\u2557', L'\u255D', L'\u255C', L'\u255B', L'\u2510',
    L'\u2514', L'\u2534', L'\u252C', L'\u251C', L'\u2500', L'\u253C', L'\u255E', L'\u255F',
    L'\u255A', L'\u2554', L'\u2569', L'\u2566', L'\u2560', L'\u2550', L'\u256C', L'\u2567',
    L'\u2568', L'\u2564', L'\u2565', L'\u2559', L'\u2558', L'\u2552', L'\u2553', L'\u256B',
    L'\u256A', L'\u2518', L'\u250C', L'\u2588', L'\u2584', L'\u258C', L'\u2590', L'\u2580',
    L'\u03B1', L'\u00DF', L'\u0393', L'\u03C0', L'\u03A3', L'\u03C3', L'\u00B5', L'\u03C4',
    L'\u03A6', L'\u0398', L'\u03A9', L'\u03B4', L'\u221E', L'\u03C6', L'\u03B5', L'\u2229',
    L'\u2261', L'\u00B1', L'\u2265', L'\u2264', L'\u2320', L'\u2321', L'\u00F7', L'\u2248',
    L'\u00B0', L'\u2219', L'\u00B7', L'\u221A', L'\u207F', L'\u00B2', L'\u25A0', L'\u00A0',
};

static wchar_t cp437_to_unicode(uint8_t c) {
    if (c < 0x20) return cp437_low[c];
    if (c == 0x7F) return L'\u2302';
    if (c >= 0x80) return cp437_high[c - 0x80];
    return static_cast<wchar_t>(c);
}

// ========================================================================
// DX11 + D2D + ImGui state
// ========================================================================

struct DxState {
    HWND hwnd = nullptr;
    int winW = 0, winH = 0;
    float cellW = 0, cellH = 0;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> ctx;
    ComPtr<IDXGISwapChain1> swapChain;
    ComPtr<ID3D11RenderTargetView> rtv;

    ComPtr<ID2D1Factory1> d2dFactory;
    ComPtr<ID2D1Device> d2dDevice;
    ComPtr<ID2D1DeviceContext> d2dCtx;
    ComPtr<ID2D1Bitmap1> d2dTarget;

    ComPtr<IDWriteFactory5> dwriteFactory;
    ComPtr<IDWriteTextFormat> textFormat;

    ComPtr<ID2D1SolidColorBrush> greenBrush;
    ComPtr<ID2D1SolidColorBrush> brightGreenBrush;
    ComPtr<ID2D1SolidColorBrush> blackBrush;
    ComPtr<ID2D1SolidColorBrush> underlineBrush;  // dim green for underline

    // MDA card (for CRTC cursor registers)
    const ISA_MDA* mda_card = nullptr;

    // Bus probe (signal pool indices for bus analyzer)
    const BusProbe* bus_probe = nullptr;
    bool bus_view_open = false;

    // PCB board view
    BoardView board_view;

    // Blink timing: derived from QPC wall clock, independent of render frame rate.
    // Real MDA field rate: 18.432 MHz dot clock / (882 chars * 370 lines) = ~56.5 Hz.
    // The 6845's 5-bit cursor_counter increments once per field.
    // We compute: fields_elapsed = (qpc_now - qpc_start) * 56.5 / qpc_freq
    // Then cursor_counter = fields_elapsed & 0x1F.
    // Character attribute blink (bit 7) uses the same counter at 1/32 field rate.
    LARGE_INTEGER qpc_freq_ = {};
    LARGE_INTEGER qpc_start_ = {};
    static constexpr double MDA_FIELD_HZ = 18432000.0 / (882.0 * 370.0);  // ~56.5 Hz

    // MHz tracking
    const uint64_t* clk_cycles = nullptr;
    uint64_t last_cycles = 0;
    std::chrono::steady_clock::time_point last_time;
    double effective_mhz = 0.0;

    // Debugger
    Scheduler* scheduler = nullptr;
    IC_8088* cpu = nullptr;
    const MemoryView* mem = nullptr;
    IC_8237A* dma = nullptr;
    bool* dbg_visible = nullptr;  // points to MdaDisplay::dbg_visible_

    // Zydis disassembler (8086 real mode)
    ZydisDecoder decoder = {};
    ZydisFormatter formatter = {};

    // Disassembly view state (DOSBox-style: persistent top-of-window address,
    // scrolls forward when IP passes the midpoint).
    uint16_t view_cs = 0;
    uint16_t view_ip = 0;
    bool view_init = false;

    // Memory viewer
    bool mem_view_open = false;
    char mem_addr_buf[16] = "0000:0000";
    uint32_t mem_view_addr = 0;

    // System window
    bool system_open = false;
    std::string drive_a_path;
    std::string drive_b_path;
    std::string drive_a_loaded;  // path currently loaded in FDC
    int drop_target_drive = 0;   // 0=A, 1=B (set by ImGui hover, read by WM_DROPFILES)
    ISA_FloppyController* fdc = nullptr;

    // Breakpoint
    char brk_addr_buf[16] = "";
    static constexpr int MEM_ROWS = 16;
    static constexpr int MEM_COLS = 16;

    bool init(HWND hwnd, int w, int h);
    void render_mda(const uint8_t* vram);
    void render_overlay();
    void render_debugger();
    void render_memory_viewer();
    void render_bus_analyzer();
    void render_system_window();
    void present();
};

bool DxState::init(HWND hw, int w, int h) {
    hwnd = hw; winW = w; winH = h;
    cellW = (float)w / MDA_COLS;
    cellH = (float)h / MDA_ROWS;

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        &fl, 1, D3D11_SDK_VERSION, &device, nullptr, &ctx);

    ComPtr<IDXGIDevice1> dxgiDevice;
    device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);
    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width = w; scd.Height = h;
    scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    factory->CreateSwapChainForHwnd(device.Get(), hwnd, &scd, nullptr, nullptr, &swapChain);

    // RTV for ImGui (it renders via DX11 directly, not D2D).
    ComPtr<ID3D11Texture2D> backBuf;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuf));
    device->CreateRenderTargetView(backBuf.Get(), nullptr, &rtv);

    // D2D
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory.GetAddressOf());
    d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
    d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dCtx);

    ComPtr<IDXGISurface> backSurface;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backSurface));
    D2D1_BITMAP_PROPERTIES1 bmpProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    d2dCtx->CreateBitmapFromDxgiSurface(backSurface.Get(), &bmpProps, &d2dTarget);
    d2dCtx->SetTarget(d2dTarget.Get());

    // DirectWrite + MDA font
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory5),
        (IUnknown**)dwriteFactory.GetAddressOf());
    ComPtr<IDWriteFontFile> fontFile;
    dwriteFactory->CreateFontFileReference(L"assets/Ac437_IBM_MDA.ttf", nullptr, &fontFile);
    ComPtr<IDWriteFontSetBuilder1> fontSetBuilder;
    dwriteFactory->CreateFontSetBuilder(&fontSetBuilder);
    fontSetBuilder->AddFontFile(fontFile.Get());
    ComPtr<IDWriteFontSet> fontSet;
    fontSetBuilder->CreateFontSet(&fontSet);
    ComPtr<IDWriteFontCollection1> fc1;
    dwriteFactory->CreateFontCollectionFromFontSet(fontSet.Get(), &fc1);

    dwriteFactory->CreateTextFormat(L"Ac437 IBM MDA", fc1.Get(),
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        cellH, L"en-us", &textFormat);
    textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    d2dCtx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.85f, 0.0f), &greenBrush);
    d2dCtx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 1.0f, 0.0f), &brightGreenBrush);
    d2dCtx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &blackBrush);
    d2dCtx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.65f, 0.0f), &underlineBrush);

    // QPC for blink timing (frame-rate independent).
    QueryPerformanceFrequency(&qpc_freq_);
    QueryPerformanceCounter(&qpc_start_);

    // ImGui -- scale font + style for high-DPI.
    // Load the font at the scaled pixel size (not FontGlobalScale, which
    // just stretches the already-rasterized atlas and looks blurry).
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    float dpi_scale = static_cast<float>(GetDpiForWindow(hwnd)) / 96.0f;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 14.0f * dpi_scale);
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device.Get(), ctx.Get());

    last_time = std::chrono::steady_clock::now();

    // Zydis: 8086 real mode disassembler
    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_REAL_16, ZYDIS_STACK_WIDTH_16);
    ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);

    // PCB board view
    if (!board_view.init(device.Get(), "assets/board_traces.json"))
        return false;

    return true;
}

void DxState::render_mda(const uint8_t* vram) {
    d2dCtx->BeginDraw();
    d2dCtx->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    // Compute blink counter from wall clock at MDA field rate (~56.5 Hz).
    LARGE_INTEGER qpc_now;
    QueryPerformanceCounter(&qpc_now);
    double elapsed_sec = double(qpc_now.QuadPart - qpc_start_.QuadPart) / qpc_freq_.QuadPart;
    uint32_t fields_elapsed = static_cast<uint32_t>(elapsed_sec * MDA_FIELD_HZ);
    uint8_t cursor_counter = fields_elapsed & 0x1F;  // 5-bit counter

    // Read CRTC cursor registers (Option A: direct read, no sync).
    // R10: cursor start scan line (bits 4:0), bits 6:5 = cursor mode
    //      00 = visible steady, 01 = invisible, 10 = blink 1/16, 11 = blink 1/32
    // R11: cursor end scan line (bits 4:0)
    // R14:R15: cursor position (character offset from start of display buffer)
    // R12:R13: display start address
    // R9: max scan line (character height - 1, normally 0x0D = 13 for 14-line chars)
    int cursor_pos = -1;
    int cursor_start_sl = 0, cursor_end_sl = 0;
    int cursor_mode = 0;  // 0=steady, 1=invisible, 2=blink/16, 3=blink/32
    int max_scanline = 13;
    int display_start = 0;
    bool cursor_visible = false;

    if (mda_card) {
        const uint8_t* cr = mda_card->crtc_regs();
        cursor_start_sl = cr[ISA_MDA::CRTC_CURSOR_START] & 0x1F;
        cursor_mode     = (cr[ISA_MDA::CRTC_CURSOR_START] >> 5) & 0x03;
        cursor_end_sl   = cr[ISA_MDA::CRTC_CURSOR_END] & 0x1F;
        cursor_pos      = (cr[ISA_MDA::CRTC_CURSOR_H] << 8) | cr[ISA_MDA::CRTC_CURSOR_L];
        display_start   = (cr[ISA_MDA::CRTC_START_ADDR_H] << 8) | cr[ISA_MDA::CRTC_START_ADDR_L];
        max_scanline    = cr[ISA_MDA::CRTC_MAX_SCANLINE] & 0x1F;
        if (max_scanline == 0) max_scanline = 13;

        // MC6845 cursor blink per IBM BIOS source (PCBIOS.ASM):
        //
        //   "** HARDWARE WILL ALWAYS CAUSE BLINK"
        //   "** SETTING BIT 5 OR 6 WILL CAUSE ERRATIC BLINKING
        //       OR NO CURSOR AT ALL"
        //
        // The cursor always blinks.  There is no steady (non-blinking) mode.
        // BIOS programs R10 = 0x0B (bits 6:5 = 00, start = 11).
        //
        //   Bits 6:5  Behavior (real MDA hardware)
        //   --------  -------------------------------------------
        //   00        Blink at 1/16 field rate (~3.75 Hz at ~60 Hz)
        //   01        Erratic / no cursor
        //   10        Erratic / no cursor
        //   11        Blink at 1/32 field rate (~1.875 Hz)
        //
        // 5-bit counter incremented once per frame (vsync).
        // Bit 5 selects counter bit: 0 -> bit 3 (fast), 1 -> bit 4 (slow).
        //
        // No wrap-around: start > end = empty scanline range = invisible.
        //
        // Ref: crtc6845.v for counter/bit-select logic (correct for blink
        //      timing, wrong about mode 00 being "always on").
        //      IBM PCBIOS.ASM for definitive mode 00 = always blink.

        if (cursor_mode == 0b01 || cursor_mode == 0b10) {
            cursor_visible = false;  // erratic / no cursor per BIOS
        } else {
            // Bit 5 selects blink rate: 0 = counter[3], 1 = counter[4].
            bool blink_sig = (cursor_mode & 1)
                ? ((cursor_counter >> 4) & 1)
                : ((cursor_counter >> 3) & 1);
            cursor_visible = blink_sig && (cursor_start_sl <= cursor_end_sl);
        }

        // Adjust cursor_pos relative to display_start.
        cursor_pos -= display_start;
    }

    // Character attribute blink: bit 7 = blink at 1/32 field rate.
    // Uses counter[4] (same as cursor mode 11), toggling every 16 fields.
    bool blink_on = ((cursor_counter >> 4) & 1) == 0;

    if (vram) {
        for (int row = 0; row < MDA_ROWS; ++row) {
            for (int col = 0; col < MDA_COLS; ++col) {
                int char_idx = row * MDA_COLS + col;
                int off = char_idx * 2;
                uint8_t ch = vram[off];
                uint8_t attr = vram[off + 1];

                float x  = floorf(col * cellW);
                float y  = floorf(row * cellH);
                float x2 = floorf((col + 1) * cellW);
                float y2 = floorf((row + 1) * cellH);
                float mx = x + (x2 - x) * 0.5f;
                float my = y + (y2 - y) * 0.5f;

                // MDA attribute decoding per IBM Technical Reference:
                //   BG(RGB) FG(RGB)  Function
                //   000     000      Non-display (invisible)
                //   000     001      Underline
                //   000     111      Normal (white on black)
                //   111     000      Reverse video
                //   Bit 3 (intensity): brighter foreground
                //   Bit 7 (blink): character blinks
                uint8_t bg_rgb = (attr >> 4) & 0x07;
                uint8_t fg_rgb = attr & 0x07;
                bool intensity = (attr & 0x08) != 0;
                bool blink_attr = (attr & 0x80) != 0;

                // If blink attribute set and we're in the off phase, hide character.
                if (blink_attr && !blink_on) {
                    // Show background only (character hidden).
                    if (bg_rgb == 0x07) {
                        // Reverse video background stays.
                        d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), greenBrush.Get());
                    }
                    // Still need to draw cursor even when char is blinked off.
                    goto draw_cursor;
                }

                if (fg_rgb == 0 && bg_rgb == 0) {
                    // Non-display: invisible. Skip character but still draw cursor.
                    goto draw_cursor;
                }

                if (bg_rgb == 0x07) {
                    // Reverse video: green background, black foreground.
                    d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), greenBrush.Get());

                    // Draw character in black.
                    if (ch != 0x20) {
                        wchar_t wc = cp437_to_unicode(ch);
                        d2dCtx->DrawText(&wc, 1, textFormat.Get(),
                            D2D1::RectF(x, y, x2, y2), blackBrush.Get());
                    }
                    goto draw_cursor;
                }

                {
                    // Normal or underline.
                    ID2D1SolidColorBrush* brush = intensity ? brightGreenBrush.Get() : greenBrush.Get();

                    // Draw character.
                    switch (ch) {
                    case 0xDB: d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), brush); break;
                    case 0xDC: d2dCtx->FillRectangle(D2D1::RectF(x, my, x2, y2), brush); break;
                    case 0xDF: d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, my), brush); break;
                    case 0xDD: d2dCtx->FillRectangle(D2D1::RectF(x, y, mx, y2), brush); break;
                    case 0xDE: d2dCtx->FillRectangle(D2D1::RectF(mx, y, x2, y2), brush); break;
                    case 0xB0:
                        brush->SetOpacity(0.25f);
                        d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), brush);
                        brush->SetOpacity(1.0f);
                        break;
                    case 0xB1:
                        brush->SetOpacity(0.50f);
                        d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), brush);
                        brush->SetOpacity(1.0f);
                        break;
                    case 0xB2:
                        brush->SetOpacity(0.75f);
                        d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), brush);
                        brush->SetOpacity(1.0f);
                        break;
                    case 0x20: break;
                    default: {
                        wchar_t wc = cp437_to_unicode(ch);
                        d2dCtx->DrawText(&wc, 1, textFormat.Get(),
                            D2D1::RectF(x, y, x2, y2), brush);
                        break;
                    }
                    }

                    // Underline: FG RGB = 001. Draw a line at scan line 12 (of 0-13).
                    if (fg_rgb == 0x01) {
                        float scanH = (y2 - y) / (max_scanline + 1);
                        float ulY = y + scanH * 12.0f;
                        d2dCtx->FillRectangle(D2D1::RectF(x, ulY, x2, ulY + scanH), brush);
                    }
                }

            draw_cursor:
                // MC6845 hardware cursor.
                // Ref: crtc6845.v -- cursor visible when scanline >= start AND <= end.
                // No wrap-around: start > end = empty range = invisible.
                // Cursor is a filled block spanning start..end scan lines.
                if (cursor_visible && char_idx == cursor_pos) {
                    float scanH = (y2 - y) / (max_scanline + 1);
                    float cy1 = y + scanH * cursor_start_sl;
                    float cy2 = y + scanH * (cursor_end_sl + 1);
                    d2dCtx->FillRectangle(D2D1::RectF(x, cy1, x2, cy2), greenBrush.Get());
                }
            }
        }
    }

    d2dCtx->EndDraw();
}

void DxState::render_overlay() {
    // Update MHz calculation every frame.
    if (clk_cycles) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_time).count();
        if (elapsed >= 0.5) {  // update every 500ms for stability
            uint64_t cur = *clk_cycles;
            double delta = static_cast<double>(cur - last_cycles);
            effective_mhz = (delta / elapsed) / 1e6;
            last_cycles = cur;
            last_time = now;
        }
    }

    // ImGui overlay
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // F12 toggles debugger panel.
    if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false) && dbg_visible)
        *dbg_visible = !*dbg_visible;

    // --- Status + Menu HUD (bottom-right) ---
    {
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing;

        ImGui::SetNextWindowBgAlpha(0.6f);
        ImGui::SetNextWindowPos(
            ImVec2((float)winW - 10.0f, (float)winH - 10.0f),
            ImGuiCond_Always,
            ImVec2(1.0f, 1.0f));

        ImGui::Begin("##stats", nullptr, flags);

        // Menu button (centered)
        {
            float btn_w = ImGui::CalcTextSize("  Menu  ").x + ImGui::GetStyle().FramePadding.x * 2;
            float avail = ImGui::GetContentRegionAvail().x;
            if (avail > btn_w) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btn_w) * 0.5f);
            if (ImGui::Button("  Menu  "))
                ImGui::OpenPopup("MainMenu");
        }

        if (ImGui::BeginPopup("MainMenu")) {
            if (ImGui::MenuItem("System"))        system_open = !system_open;
            if (ImGui::MenuItem("Board"))         board_view.toggle();
            if (ImGui::MenuItem("Debugger"))      { if (dbg_visible) *dbg_visible = !*dbg_visible; }
            if (ImGui::MenuItem("Bus"))           bus_view_open = !bus_view_open;
            if (ImGui::MenuItem("Memory"))        mem_view_open = !mem_view_open;
            ImGui::EndPopup();
        }

        // Status: PAUSED or MHz (clickable to toggle)
        bool paused = scheduler && scheduler->is_paused();
        if (paused)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "PAUSED");
        else
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%.2f MHz", effective_mhz);
        if (scheduler && ImGui::IsItemClicked()) {
            if (paused) scheduler->resume(); else scheduler->pause();
            if (dbg_visible) *dbg_visible = !paused;
        }
        ImGui::End();
    }

    // --- Debugger panel (top-left, translucent) ---
    if (dbg_visible && *dbg_visible && scheduler)
        render_debugger();

    // --- Memory viewer (separate window) ---
    if (mem_view_open && mem)
        render_memory_viewer();

    // --- Bus analyzer (separate window) ---
    if (bus_view_open && bus_probe)
        render_bus_analyzer();

    // --- System window ---
    if (system_open)
        render_system_window();

    // --- PCB board view (F2 toggle) ---
    if (ImGui::IsKeyPressed(ImGuiKey_F2, false))
        board_view.toggle();
    board_view.imgui_window(ctx.Get());

    ImGui::Render();

    ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DxState::render_debugger() {
    static constexpr int DISASM_LINES = 21;  // total visible lines
    static constexpr int MID_LINE = DISASM_LINES / 2;  // IP target row

    // Layout:  2 toolbar rows + 1 separator + 1 CLK line + 3 register lines +
    //          1 flags + 1 separator + DISASM_LINES disasm = DISASM_LINES + 8 content lines
    // Plus title bar + frame padding.
    static constexpr int CONTENT_LINES = DISASM_LINES + 8;
    // "F000:FFFF  FF FF FF FF FF FF  mov word [bp+si+0x1234], 0x5678"
    // = ~60 chars.  Consolas at 14px base: char width ~ 8.4px * dpi_scale.
    static constexpr int LINE_CHARS = 62;

    ImGuiWindowFlags dbg_flags =
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize;

    float line_h = ImGui::GetTextLineHeightWithSpacing();
    float char_w = ImGui::CalcTextSize("X").x;
    float pad = ImGui::GetStyle().WindowPadding.x * 2;
    float title_h = ImGui::GetFrameHeight();  // title bar
    float w = char_w * LINE_CHARS + pad;
    float h = line_h * CONTENT_LINES + title_h + pad;

    ImGui::SetNextWindowBgAlpha(0.90f);
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(w, h));

    ImGui::Begin("Debugger [`]", dbg_visible, dbg_flags);

    bool paused = scheduler->is_paused();

    // --- Toolbar ---
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (paused) scheduler->resume(); else scheduler->pause();
        paused = !paused;
    }
    {
        float btn_w = ImGui::CalcTextSize("Resume (F5)").x + ImGui::GetStyle().FramePadding.x * 2;
        if (paused) {
            if (ImGui::Button("Resume (F5)", ImVec2(btn_w, 0))) scheduler->resume();
        } else {
            if (ImGui::Button("Pause  (F5)", ImVec2(btn_w, 0))) scheduler->pause();
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!paused);
    // F9: Step Over -- run until next instruction at same stack level
    bool do_step_over = ImGui::Button("Over (F9)");
    if (paused && ImGui::IsKeyPressed(ImGuiKey_F9, true))
        do_step_over = true;
    if (do_step_over && cpu && mem) {
        uint16_t cs = cpu->regs16_ro()[IC_8088::CS];
        uint16_t ip = cpu->ip();
        uint16_t sp = cpu->regs16_ro()[IC_8088::SP];
        uint32_t phys = ((uint32_t)cs << 4) + ip;
        uint8_t buf[15];
        mem->read(phys & 0xFFFFF, buf, 15);
        ZydisDecodedInstruction instr;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        uint16_t len = 1;
        if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buf, 15, &instr, operands)))
            len = (uint16_t)instr.length;
        scheduler->step_over(ip + len, sp);
    }
    ImGui::SameLine();
    // F10: Step Into -- single instruction
    bool do_step_instr = ImGui::Button("Into (F10)");
    if (paused && ImGui::IsKeyPressed(ImGuiKey_F10, true))
        do_step_instr = true;
    if (do_step_instr)
        scheduler->step_instruction();
    ImGui::SameLine();
    // F11: Cycle -- single CLK cycle
    bool do_step_cycle = ImGui::Button("Cycle (F11)");
    if (paused && ImGui::IsKeyPressed(ImGuiKey_F11, true))
        do_step_cycle = true;
    if (do_step_cycle)
        scheduler->step_cycle();
    ImGui::EndDisabled();

    // --- Second toolbar row: breakpoint, T-state, DMA ---
    ImGui::Text("Break:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90);
    if (ImGui::InputText("##brk", brk_addr_buf, sizeof(brk_addr_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        unsigned seg = 0, off = 0;
        uint32_t linear = UINT32_MAX;
        if (sscanf(brk_addr_buf, "%x:%x", &seg, &off) == 2)
            linear = ((seg << 4) + off) & 0xFFFFF;
        else if (sscanf(brk_addr_buf, "%x", &off) == 1)
            linear = off & 0xFFFFF;
        scheduler->set_break_address(linear);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("seg:off or linear. Enter to set.");

    if (!cpu) { ImGui::End(); return; }

    {
        static const char* t_names[] = {"Ti", "T1", "T2", "T3", "Tw", "T4"};
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
            t_names[static_cast<int>(cpu->t_state())]);
    }
    if (dma) {
        static const char* dma_names[] = {
            "SI", "BREQ", "S1", "S2", "S3", "S4",
            "M2M-S1", "M2M-S2", "M2M-S3", "M2M-S4"
        };
        ImGui::SameLine();
        const char* ds = dma_names[static_cast<int>(dma->state())];
        int ch = dma->active_channel();
        if (ch >= 0)
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DMA:%s/CH%d", ds, ch);
        else
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DMA:%s", ds);
    }

    ImGui::Separator();
    if (clk_cycles)
        ImGui::Text("CLK: %" PRIu64 "  %.2f MHz", *clk_cycles, effective_mhz);

    const uint16_t* r = cpu->regs16_ro();
    const uint8_t* r8 = cpu->regs8_ro();
    using R = IC_8088::Reg16;
    using F = IC_8088::Flag;

    // --- Registers (segment:offset pairs) ---
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
    ImGui::Text("AX=%04X  BX=%04X  CX=%04X  DX=%04X  BP=%04X",
                 r[R::AX], r[R::BX], r[R::CX], r[R::DX], r[R::BP]);
    ImGui::Text("CS:IP=%04X:%04X  DS:SI=%04X:%04X",
                 r[R::CS], cpu->ip(), r[R::DS], r[R::SI]);
    ImGui::Text("SS:SP=%04X:%04X  ES:DI=%04X:%04X",
                 r[R::SS], r[R::SP], r[R::ES], r[R::DI]);

    char fl[] = "---------";
    if (r8[F::OF]) fl[0] = 'O'; if (r8[F::DF]) fl[1] = 'D';
    if (r8[F::IF]) fl[2] = 'I'; if (r8[F::TF]) fl[3] = 'T';
    if (r8[F::SF]) fl[4] = 'S'; if (r8[F::ZF]) fl[5] = 'Z';
    if (r8[F::AF]) fl[6] = 'A'; if (r8[F::PF]) fl[7] = 'P';
    if (r8[F::CF]) fl[8] = 'C';
    ImGui::Text("%s", fl);
    ImGui::PopStyleColor();

    // --- Disassembly (DOSBox-style persistent view) ---
    if (!mem || !paused) { ImGui::End(); return; }

    ImGui::Separator();

    uint16_t cs = r[R::CS];
    uint16_t ip = cpu->ip();
    uint32_t cs_base = (uint32_t)cs << 4;

    // Initialize or re-sync view when CS changes or IP jumps out of view.
    if (!view_init || view_cs != cs) {
        view_cs = cs;
        view_ip = ip;
        view_init = true;
    }

    // Disassemble DISASM_LINES from view_ip, find which line IP falls on.
    struct DisLine { uint16_t addr; uint8_t len; char hex[32]; char text[128]; };
    DisLine lines[DISASM_LINES];
    int ip_line = -1;

    uint16_t cur = view_ip;
    for (int i = 0; i < DISASM_LINES; ++i) {
        lines[i].addr = cur;
        if (cur == ip) ip_line = i;

        uint8_t buf[15];
        mem->read((cs_base + cur) & 0xFFFFF, buf, 15);

        ZydisDecodedInstruction instr;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buf, 15, &instr, operands))) {
            ZydisFormatterFormatInstruction(&formatter, &instr, operands,
                instr.operand_count, lines[i].text, sizeof(lines[i].text),
                (uint64_t)cs_base + cur, ZYAN_NULL);
            lines[i].len = (uint8_t)instr.length;
            int hpos = 0;
            for (int b = 0; b < (int)instr.length && hpos < 28; ++b)
                hpos += snprintf(lines[i].hex + hpos, 32 - hpos, "%02X ", buf[b]);
        } else {
            snprintf(lines[i].hex, 32, "%02X", buf[0]);
            snprintf(lines[i].text, 128, "db 0x%02X", buf[0]);
            lines[i].len = 1;
        }
        cur += lines[i].len;
    }

    // Scroll the view so IP stays near the middle.
    // If IP is below the window, advance view_ip until IP is at MID_LINE.
    // If IP is above the window, snap view_ip to IP.
    if (ip_line < 0) {
        // IP not in view -- snap to IP centered.
        view_ip = ip;
    } else if (ip_line > MID_LINE) {
        // IP drifted below midpoint -- scroll forward.
        // Advance view_ip by the sizes of the lines we're scrolling past.
        int scroll = ip_line - MID_LINE;
        for (int i = 0; i < scroll; ++i)
            view_ip += lines[i].len;
    }
    // If we adjusted, re-disassemble so this frame is correct.
    if (ip_line != MID_LINE && ip_line >= 0 && ip_line <= MID_LINE) {
        // IP is above midpoint but still in view -- leave it, natural scroll.
    } else if (ip_line < 0 || ip_line > MID_LINE) {
        // Re-disassemble with updated view_ip.
        cur = view_ip;
        ip_line = -1;
        for (int i = 0; i < DISASM_LINES; ++i) {
            lines[i].addr = cur;
            if (cur == ip) ip_line = i;

            uint8_t buf[15];
            mem->read((cs_base + cur) & 0xFFFFF, buf, 15);

            ZydisDecodedInstruction instr;
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
            if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder, buf, 15, &instr, operands))) {
                ZydisFormatterFormatInstruction(&formatter, &instr, operands,
                    instr.operand_count, lines[i].text, sizeof(lines[i].text),
                    (uint64_t)cs_base + cur, ZYAN_NULL);
                lines[i].len = (uint8_t)instr.length;
                int hpos = 0;
                for (int b = 0; b < (int)instr.length && hpos < 28; ++b)
                    hpos += snprintf(lines[i].hex + hpos, 32 - hpos, "%02X ", buf[b]);
            } else {
                snprintf(lines[i].hex, 32, "%02X", buf[0]);
                snprintf(lines[i].text, 128, "db 0x%02X", buf[0]);
                lines[i].len = 1;
            }
            cur += lines[i].len;
        }
    }

    // --- Render ---
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < DISASM_LINES; ++i) {
        bool is_ip = (lines[i].addr == ip);

        if (is_ip) {
            // Highlight bar behind current instruction
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float w = ImGui::GetContentRegionAvail().x;
            dl->AddRectFilled(
                ImVec2(pos.x - 4, pos.y),
                ImVec2(pos.x + w + 4, pos.y + line_h),
                IM_COL32(60, 60, 20, 220));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f),
                "%04X:%04X  %-18s %s", cs, lines[i].addr,
                lines[i].hex, lines[i].text);
        } else {
            ImGui::TextColored(ImVec4(0.50f, 0.65f, 0.50f, 1.0f),
                "%04X:%04X  %-18s %s", cs, lines[i].addr,
                lines[i].hex, lines[i].text);
        }
    }

    ImGui::End();
}

void DxState::render_memory_viewer() {
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::SetNextWindowSize(ImVec2(580, 380), ImGuiCond_Once);

    if (!ImGui::Begin("Memory", &mem_view_open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    // Address input: accepts "SSSS:OOOO" or "XXXXX" (linear)
    ImGui::SetNextItemWidth(120);
    if (ImGui::InputText("##addr", mem_addr_buf, sizeof(mem_addr_buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        unsigned seg = 0, off = 0;
        if (sscanf(mem_addr_buf, "%x:%x", &seg, &off) == 2)
            mem_view_addr = ((seg << 4) + off) & 0xFFFFF;
        else if (sscanf(mem_addr_buf, "%x", &off) == 1)
            mem_view_addr = off & 0xFFFFF;
    }
    ImGui::SameLine();
    // Quick-jump buttons
    if (cpu) {
        const uint16_t* r = cpu->regs16_ro();
        if (ImGui::Button("SS:SP")) {
            mem_view_addr = ((r[IC_8088::SS] << 4) + r[IC_8088::SP]) & 0xFFFFF;
            snprintf(mem_addr_buf, sizeof(mem_addr_buf), "%04X:%04X",
                     r[IC_8088::SS], r[IC_8088::SP]);
        }
        ImGui::SameLine();
        if (ImGui::Button("DS:SI")) {
            mem_view_addr = ((r[IC_8088::DS] << 4) + r[IC_8088::SI]) & 0xFFFFF;
            snprintf(mem_addr_buf, sizeof(mem_addr_buf), "%04X:%04X",
                     r[IC_8088::DS], r[IC_8088::SI]);
        }
        ImGui::SameLine();
        if (ImGui::Button("ES:DI")) {
            mem_view_addr = ((r[IC_8088::ES] << 4) + r[IC_8088::DI]) & 0xFFFFF;
            snprintf(mem_addr_buf, sizeof(mem_addr_buf), "%04X:%04X",
                     r[IC_8088::ES], r[IC_8088::DI]);
        }
        ImGui::SameLine();
        if (ImGui::Button("CS:IP")) {
            mem_view_addr = ((r[IC_8088::CS] << 4) + cpu->ip()) & 0xFFFFF;
            snprintf(mem_addr_buf, sizeof(mem_addr_buf), "%04X:%04X",
                     r[IC_8088::CS], cpu->ip());
        }
    }

    // Mouse wheel scrolling (1 row = 16 bytes)
    float wheel = ImGui::GetIO().MouseWheel;
    if (ImGui::IsWindowHovered() && wheel != 0.0f) {
        int delta = -(int)wheel * MEM_COLS;
        mem_view_addr = (mem_view_addr + delta) & 0xFFFFF;
    }

    ImGui::Separator();

    // Header
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
        "         00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII");

    // Rows
    for (int row = 0; row < MEM_ROWS; ++row) {
        uint32_t row_addr = (mem_view_addr + row * MEM_COLS) & 0xFFFFF;
        uint8_t data[16];
        mem->read(row_addr, data, MEM_COLS);

        char hex[64];
        int p = 0;
        for (int i = 0; i < 16; ++i) {
            if (i == 8) hex[p++] = ' ';
            p += snprintf(hex + p, sizeof(hex) - p, "%02X ", data[i]);
        }

        char ascii[17];
        for (int i = 0; i < 16; ++i)
            ascii[i] = (data[i] >= 0x20 && data[i] < 0x7F) ? (char)data[i] : '.';
        ascii[16] = '\0';

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
            "%05X  %s %s", row_addr, hex, ascii);
    }

    ImGui::End();
}

void DxState::render_bus_analyzer() {
    const auto& bp = *bus_probe;
    const auto* L = SignalPool::levels;

    // Helper: read N contiguous levels as hex value (High=1 bit set)
    auto bus_hex = [&](int base, int n) -> uint32_t {
        uint32_t v = 0;
        for (int i = 0; i < n; ++i)
            if (L[base + i] == Level::High) v |= (1u << i);
        return v;
    };
    // Helper: signal level as char
    auto lch = [&](int idx) -> char {
        Level v = L[idx];
        return v == Level::High ? 'H' : v == Level::Low ? 'L' : 'Z';
    };
    // Helper: active-low signal as colored text
    auto active_low = [&](const char* name, int idx) {
        Level v = L[idx];
        bool active = (v == Level::Low);
        ImVec4 col = active ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        ImGui::TextColored(col, "%s=%c", name, lch(idx));
    };

    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::SetNextWindowSize(ImVec2(420, 340), ImGuiCond_Once);

    if (!ImGui::Begin("Bus Analyzer", &bus_view_open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    ImVec4 grn = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    ImVec4 dim = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    ImVec4 yel = ImVec4(1.0f, 1.0f, 0.3f, 1.0f);
    ImVec4 red = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
    ImVec4 cyn = ImVec4(0.4f, 1.0f, 1.0f, 1.0f);

    // Address/data buses
    uint32_t la_val = bus_hex(bp.la, 20);
    uint32_t ad_val = bus_hex(bp.ad, 8);
    uint32_t d_val  = bus_hex(bp.d, 8);
    uint32_t xd_val = bus_hex(bp.xd, 8);
    uint32_t md_val = bus_hex(bp.md, 8);

    // --- Summary line: decode bus cycle type ---
    {
        // S2 S1 S0 decode (active-low pins: L on pin = 0 in value)
        //  0  0  0 = INTA       0  0  1 = IOR
        //  0  1  0 = IOW        0  1  1 = HALT
        //  1  0  0 = FETCH      1  0  1 = MEMR
        //  1  1  0 = MEMW       1  1  1 = passive
        int s2 = (L[bp.s2] == Level::Low) ? 0 : 1;
        int s1 = (L[bp.s1] == Level::Low) ? 0 : 1;
        int s0 = (L[bp.s0] == Level::Low) ? 0 : 1;
        int code = (s2 << 2) | (s1 << 1) | s0;
        static const char* cycle_names[] = {
            "INTA", "IOR", "IOW", "HALT", "FETCH", "MEMR", "MEMW", "---"
        };
        const char* cycle = cycle_names[code];
        bool is_read = (code == 1 || code == 4 || code == 5);  // IOR, FETCH, MEMR
        bool is_write = (code == 2 || code == 6);               // IOW, MEMW
        bool is_active = (code < 7);

        if (is_active) {
            uint8_t data = is_read ? (uint8_t)d_val : (uint8_t)d_val;
            ImGui::TextColored(cyn, "%s  %05X  D=%02X", cycle, la_val, data);
            if (is_read) {
                ImGui::SameLine();
                ImGui::TextColored(cyn, " (reading)");
            } else if (is_write) {
                ImGui::SameLine();
                ImGui::TextColored(red, " (writing)");
            }
        } else {
            bool dma_owns = L[bp.aen_brd] == Level::High;
            ImGui::TextColored(dim, "%s  %s", dma_owns ? "DMA owns bus" : "idle", cycle);
        }
        ImGui::Separator();
    }

    // Last completed bus transaction (from CPU)
    if (cpu) {
        static const char* tx_names[] = {
            "INTA", "IOR", "IOW", "HALT", "FETCH", "MEMR", "MEMW", "---"
        };
        auto& tx = cpu->last_bus_tx();
        ImGui::TextColored(yel, "Last: %-5s [%05X] = %02X",
                           tx_names[tx.type & 7], tx.addr, tx.data);
        ImGui::Separator();
    }

    ImGui::TextColored(grn, "LA[19:0]=%05X   AD[7:0]=%02X", la_val, ad_val);
    ImGui::TextColored(grn, " D[7:0] =%02X     XD[7:0]=%02X    MD[7:0]=%02X", d_val, xd_val, md_val);

    ImGui::Separator();

    // 8288 bus control
    ImGui::Text("8288:");
    ImGui::SameLine(); active_low("~MEMR", bp.memr);
    ImGui::SameLine(); active_low("~MEMW", bp.memw);
    ImGui::SameLine(); active_low("~IOR", bp.ior);
    ImGui::SameLine(); active_low("~IOW", bp.iow);

    ImGui::Text("     ");
    ImGui::SameLine(); active_low("ALE", bp.ale);   // active high actually
    ImGui::SameLine(); active_low("~DEN", bp.den);
    ImGui::SameLine();
    ImGui::TextColored(dim, "DT/~R=%c", lch(bp.dtr));

    ImGui::Separator();

    // CPU status
    ImGui::Text("CPU: ");
    ImGui::SameLine();
    ImGui::TextColored(grn, "~S2=%c ~S1=%c ~S0=%c", lch(bp.s2), lch(bp.s1), lch(bp.s0));
    ImGui::SameLine();
    ImGui::TextColored(dim, " READY=%c", lch(bp.ready));

    ImGui::Separator();

    // DMA
    bool hrq_active = L[bp.hrq] == Level::High;
    bool holda_active = L[bp.holda] == Level::High;
    bool aen_active = L[bp.aen_brd] == Level::High;
    ImGui::TextColored(hrq_active ? yel : dim, "HRQ=%c", lch(bp.hrq));
    ImGui::SameLine();
    ImGui::TextColored(holda_active ? yel : dim, "HOLDA=%c", lch(bp.holda));
    ImGui::SameLine();
    ImGui::TextColored(aen_active ? yel : dim, "AEN_BRD=%c", lch(bp.aen_brd));
    ImGui::SameLine();
    active_low("~AEN", bp.aen_bar);

    ImGui::Text("DRQ: %c%c%c%c  ~DACK: %c%c%c%c",
        lch(bp.drq0), lch(bp.drq1), lch(bp.drq2), lch(bp.drq3),
        lch(bp.dack0), lch(bp.dack1), lch(bp.dack2), lch(bp.dack3));

    ImGui::Separator();

    // Interrupts
    ImGui::TextColored(L[bp.intr] == Level::High ? yel : dim, "INTR=%c", lch(bp.intr));
    ImGui::SameLine();
    ImGui::TextColored(L[bp.nmi] == Level::High ? yel : dim, "NMI=%c", lch(bp.nmi));
    ImGui::SameLine();
    ImGui::TextColored(dim, "CLK=%c  RESET=%c", lch(bp.clk), lch(bp.reset));

    ImGui::End();
}

void DxState::render_system_window() {
    ImGui::SetNextWindowSize(ImVec2(480, 280), ImGuiCond_Once);

    if (!ImGui::Begin("System", &system_open, ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    ImVec4 grn = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
    ImVec4 dim = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

    // --- Machine info ---
    ImGui::TextColored(grn, "IBM PC 5150");
    ImGui::TextColored(dim, "CPU: Intel 8088 @ %.2f MHz", effective_mhz);
    ImGui::TextColored(dim, "RAM: 256 KB");
    ImGui::TextColored(dim, "Display: MDA 80x25");

    ImGui::Separator();
    ImGui::TextColored(grn, "Floppy Drives");

    // --- Drive A: ---
    auto drive_row = [&](const char* label, int drive_idx, std::string& path) {
        ImGui::Text("%s", label);
        ImGui::SameLine();

        // Truncate display path to filename
        std::string display = path.empty() ? "(empty)" : path;
        size_t slash = display.find_last_of("/\\");
        if (slash != std::string::npos) display = display.substr(slash + 1);
        if (display.size() > 30) display = "..." + display.substr(display.size() - 27);

        ImGui::TextColored(path.empty() ? dim : grn, "%s", display.c_str());

        // Track drop target: if mouse is on this row, drops go here
        ImVec2 row_min = ImGui::GetItemRectMin();
        ImVec2 row_max = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetItemRectMax().y);
        if (ImGui::IsMouseHoveringRect(row_min, row_max))
            drop_target_drive = drive_idx;

        ImGui::SameLine();
        char btn_id[16];
        snprintf(btn_id, sizeof(btn_id), "...##%s", label);
        if (ImGui::Button(btn_id)) {
            nfdchar_t* out = nullptr;
            nfdresult_t result = NFD_OpenDialog("img", nullptr, &out);
            if (result == NFD_OKAY && out) {
                path = out;
                free(out);
                spdlog::info("[System] {} = {}", label, path);
            }
        }
        ImGui::SameLine();
        char eject_id[16];
        snprintf(eject_id, sizeof(eject_id), "Eject##%s", label);
        if (ImGui::Button(eject_id)) {
            path.clear();
            spdlog::info("[System] {} ejected", label);
        }
    };

    drive_row("A:", 0, drive_a_path);
    drive_row("B:", 1, drive_b_path);

    // Hot-swap: if drive A path changed, reload the FDC image
    if (fdc && drive_a_path != drive_a_loaded) {
        drive_a_loaded = drive_a_path;
        if (drive_a_path.empty()) {
            fdc->load_image({}, 9, 2);
            spdlog::info("[System] Drive A: ejected");
        } else {
            std::ifstream f(drive_a_path, std::ios::binary | std::ios::ate);
            if (f) {
                auto sz = f.tellg();
                std::vector<uint8_t> img(static_cast<size_t>(sz));
                f.seekg(0);
                f.read(reinterpret_cast<char*>(img.data()), sz);
                // Detect geometry: 360K=9spt/2hd, 720K=9spt/2hd, 1.2M=15spt/2hd, 1.44M=18spt/2hd
                int spt = 9, hds = 2;
                if (sz > 400000) { spt = 15; hds = 2; }
                if (sz > 1300000) { spt = 18; hds = 2; }
                fdc->load_image(std::move(img), spt, hds);
                spdlog::info("[System] Drive A: loaded {} ({} bytes, {}spt/{}hd)",
                             drive_a_path, (int)sz, spt, hds);
            } else {
                spdlog::warn("[System] Drive A: failed to open {}", drive_a_path);
            }
        }
    }

    ImGui::Separator();
    ImGui::TextColored(dim, "Drop .img files onto drive labels to mount.");

    ImGui::End();
}

void DxState::present() {
    swapChain->Present(1, 0);
}

// ========================================================================
// Window
// ========================================================================

static TestKeyboard* s_kbd = nullptr;  // set by render_loop before window creation
static DxState* s_dx = nullptr;       // for WM_DROPFILES handler

static LRESULT CALLBACK MdaWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    // F10 generates WM_SYSKEYDOWN -- don't let DefWindowProc eat it for
    // menu activation, otherwise F10 requires two presses.
    if ((msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) && wp == VK_F10)
        return 0;

    // Forward keyboard events to the emulated keyboard.
    // Only when ImGui doesn't want keyboard input (not typing in a text field).
    if (s_kbd && ImGui::GetCurrentContext() && !ImGui::GetIO().WantCaptureKeyboard) {
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            uint8_t xt = TestKeyboard::vk_to_xt((int)wp);
            if (xt) {
                spdlog::info("[KBD] VK 0x{:02X} -> XT make 0x{:02X}", (int)wp, xt);
                s_kbd->inject_key(xt);
            }
        } else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
            uint8_t xt = TestKeyboard::vk_to_xt((int)wp);
            if (xt) {
                spdlog::info("[KBD] VK 0x{:02X} -> XT break 0x{:02X}", (int)wp, xt | 0x80);
                s_kbd->inject_key(xt | 0x80);
            }
        }
    }

    if (msg == WM_DROPFILES && s_dx) {
        HDROP hDrop = (HDROP)wp;
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(hDrop, 0, path, MAX_PATH)) {
            char utf8[MAX_PATH * 3];
            WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), nullptr, nullptr);
            std::string p(utf8);
            auto& target = (s_dx->drop_target_drive == 1) ? s_dx->drive_b_path : s_dx->drive_a_path;
            const char* label = (s_dx->drop_target_drive == 1) ? "B:" : "A:";
            target = p;
            spdlog::info("[System] Drop -> {} {}", label, p);
        }
        DragFinish(hDrop);
        return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void MdaDisplay::render_loop(std::stop_token stop) {
    s_kbd = kbd_;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int monW = GetSystemMetrics(SM_CXSCREEN);
    int monH = GetSystemMetrics(SM_CYSCREEN);
    int winH = monH * 2 / 3;
    int winW = winH * 4 / 3;
    int x = (monW - winW) / 2;
    int y = (monH - winH) / 2;

    RECT wr = { 0, 0, winW, winH };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = MdaWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"BenchMDA";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowW(L"BenchMDA", L"IBM 5150 - MDA",
        WS_OVERLAPPEDWINDOW, x, y,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, wc.hInstance, nullptr);
    DragAcceptFiles(hwnd, TRUE);

    DxState dx;
    s_dx = &dx;
    dx.clk_cycles = clk_cycles_;
    dx.scheduler = scheduler_;
    dx.cpu = cpu_;
    dx.mem = mem_;
    dx.dma = dma_;
    dx.mda_card = mda_card_;
    dx.bus_probe = bus_probe_;
    dx.dbg_visible = &dbg_visible_;
    dx.drive_a_path = "assets/IBM DOS 3.30 360K Disks - Disk 01.img";
    dx.drive_a_loaded = dx.drive_a_path;
    dx.fdc = fdc_;
    if (!dx.init(hwnd, winW, winH)) {
        spdlog::error("[MDA Display] Failed to init DX11");
        return;
    }

    ShowWindow(hwnd, SW_SHOW);
    running_.store(true);
    ready_.count_down();

    MSG msg = {};
    while (!stop.stop_requested() && msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Pick up pending board signal binding from main thread.
            if (brd_map_ready_.load(std::memory_order_acquire)) {
                dx.board_view.bind_signals(pending_brd_map_);
                brd_map_ready_.store(false, std::memory_order_release);
            }

            dx.render_mda(vram_);
            dx.render_overlay();
            dx.present();
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    s_dx = nullptr;
    DestroyWindow(hwnd);
    running_.store(false);
}

void MdaDisplay::start(const uint8_t* vram, const uint64_t* clk_cycles,
                       Scheduler* scheduler, IC_8088* cpu,
                       const MemoryView* mem, IC_8237A* dma,
                       const ISA_MDA* mda_card,
                       const BusProbe* bus,
                       TestKeyboard* kbd,
                       ISA_FloppyController* fdc) {
    vram_ = vram;
    clk_cycles_ = clk_cycles;
    scheduler_ = scheduler;
    cpu_ = cpu;
    mem_ = mem;
    dma_ = dma;
    mda_card_ = mda_card;
    bus_probe_ = bus;
    kbd_ = kbd;
    fdc_ = fdc;
    thread_ = std::jthread([this](std::stop_token stop) {
        render_loop(stop);
    });
    ready_.wait();
}

void MdaDisplay::bind_board_signals(const std::unordered_map<std::string, int>& brd_map) {
    pending_brd_map_ = brd_map;
    brd_map_ready_.store(true, std::memory_order_release);
}

void MdaDisplay::stop() {
    if (thread_.joinable()) {
        thread_.request_stop();
        if (running_.load())
            PostThreadMessage(GetThreadId(thread_.native_handle()), WM_QUIT, 0, 0);
        thread_.join();
    }
}

} // namespace bench
