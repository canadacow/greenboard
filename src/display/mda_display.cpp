// MDA Display — DX11 + D2D + ImGui overlay.

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
#include "core/scheduler.h"
#include "ic/ic_8088.h"
#include "debug/memory_view.h"
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

    // MHz tracking
    const uint64_t* clk_cycles = nullptr;
    uint64_t last_cycles = 0;
    std::chrono::steady_clock::time_point last_time;
    double effective_mhz = 0.0;

    // Debugger
    Scheduler* scheduler = nullptr;
    IC_8088* cpu = nullptr;
    const MemoryView* mem = nullptr;
    bool* dbg_visible = nullptr;  // points to MdaDisplay::dbg_visible_

    // Zydis disassembler (8086 real mode)
    ZydisDecoder decoder = {};
    ZydisFormatter formatter = {};

    // Disassembly view state (DOSBox-style: persistent top-of-window address,
    // scrolls forward when IP passes the midpoint).
    uint16_t view_cs = 0;
    uint16_t view_ip = 0;
    bool view_init = false;

    bool init(HWND hwnd, int w, int h);
    void render_mda(const uint8_t* vram);
    void render_overlay();
    void render_debugger();
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

    return true;
}

void DxState::render_mda(const uint8_t* vram) {
    d2dCtx->BeginDraw();
    d2dCtx->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    if (vram) {
        for (int row = 0; row < MDA_ROWS; ++row) {
            for (int col = 0; col < MDA_COLS; ++col) {
                int off = (row * MDA_COLS + col) * 2;
                uint8_t ch = vram[off];
                uint8_t attr = vram[off + 1];
                if (attr == 0x00) continue;

                float x  = floorf(col * cellW);
                float y  = floorf(row * cellH);
                float x2 = floorf((col + 1) * cellW);
                float y2 = floorf((row + 1) * cellH);
                float mx = x + (x2 - x) * 0.5f;
                float my = y + (y2 - y) * 0.5f;

                ID2D1SolidColorBrush* brush = greenBrush.Get();
                if (attr & 0x08) brush = brightGreenBrush.Get();

                if ((attr & 0x77) == 0x70) {
                    d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), greenBrush.Get());
                    brush = blackBrush.Get();
                }

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

    // --- Stats HUD (bottom-right, passive) ---
    {
        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing;

        ImGui::SetNextWindowBgAlpha(0.4f);
        ImGui::SetNextWindowPos(
            ImVec2((float)winW - 10.0f, (float)winH - 10.0f),
            ImGuiCond_Always,
            ImVec2(1.0f, 1.0f));

        ImGui::Begin("##stats", nullptr, flags);
        bool paused = scheduler && scheduler->is_paused();
        if (paused)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "PAUSED");
        else
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%.2f MHz", effective_mhz);
        ImGui::End();
    }

    // --- Debugger panel (top-left, translucent) ---
    if (dbg_visible && *dbg_visible && scheduler)
        render_debugger();

    ImGui::Render();

    ctx->OMSetRenderTargets(1, rtv.GetAddressOf(), nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void DxState::render_debugger() {
    static constexpr int DISASM_LINES = 21;  // total visible lines
    static constexpr int MID_LINE = DISASM_LINES / 2;  // IP target row

    ImGuiWindowFlags dbg_flags =
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoCollapse;

    float line_h = ImGui::GetTextLineHeightWithSpacing();
    float initial_w = 620.0f;
    float initial_h = line_h * (DISASM_LINES + 8) + 20.0f;

    ImGui::SetNextWindowBgAlpha(0.90f);
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(initial_w, initial_h), ImGuiCond_Once);

    ImGui::Begin("Debugger [`]", dbg_visible, dbg_flags);

    bool paused = scheduler->is_paused();

    // --- Toolbar ---
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (paused) scheduler->resume(); else scheduler->pause();
        paused = !paused;
    }
    if (paused) {
        if (ImGui::Button("Resume (F5)")) scheduler->resume();
    } else {
        if (ImGui::Button("Pause (F5)")) scheduler->pause();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!paused);
    bool do_step = ImGui::Button("Step (F10)");
    if (paused && ImGui::IsKeyPressed(ImGuiKey_F10, true))
        do_step = true;
    if (do_step) scheduler->step();
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (clk_cycles)
        ImGui::Text("CLK: %" PRIu64 "  %.2f MHz", *clk_cycles, effective_mhz);

    if (!cpu) { ImGui::End(); return; }

    const uint16_t* r = cpu->regs16_ro();
    const uint8_t* r8 = cpu->regs8_ro();
    using R = IC_8088::Reg16;
    using F = IC_8088::Flag;

    // --- Registers ---
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
    ImGui::Text("AX=%04X  BX=%04X  CX=%04X  DX=%04X",
                 r[R::AX], r[R::BX], r[R::CX], r[R::DX]);
    ImGui::Text("SP=%04X  BP=%04X  SI=%04X  DI=%04X",
                 r[R::SP], r[R::BP], r[R::SI], r[R::DI]);
    ImGui::Text("CS=%04X  DS=%04X  ES=%04X  SS=%04X  IP=%04X",
                 r[R::CS], r[R::DS], r[R::ES], r[R::SS], cpu->ip());

    char fl[] = "---------";
    if (r8[F::OF]) fl[0] = 'O'; if (r8[F::DF]) fl[1] = 'D';
    if (r8[F::IF]) fl[2] = 'I'; if (r8[F::TF]) fl[3] = 'T';
    if (r8[F::SF]) fl[4] = 'S'; if (r8[F::ZF]) fl[5] = 'Z';
    if (r8[F::AF]) fl[6] = 'A'; if (r8[F::PF]) fl[7] = 'P';
    if (r8[F::CF]) fl[8] = 'C';
    ImGui::Text("FLAGS=%s", fl);
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

void DxState::present() {
    swapChain->Present(1, 0);
}

// ========================================================================
// Window
// ========================================================================

static LRESULT CALLBACK MdaWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void MdaDisplay::render_loop(std::stop_token stop) {
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

    DxState dx;
    dx.clk_cycles = clk_cycles_;
    dx.scheduler = scheduler_;
    dx.cpu = cpu_;
    dx.mem = mem_;
    dx.dbg_visible = &dbg_visible_;
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
            dx.render_mda(vram_);
            dx.render_overlay();
            dx.present();
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyWindow(hwnd);
    running_.store(false);
}

void MdaDisplay::start(const uint8_t* vram, const uint64_t* clk_cycles,
                       Scheduler* scheduler, IC_8088* cpu,
                       const MemoryView* mem) {
    vram_ = vram;
    clk_cycles_ = clk_cycles;
    scheduler_ = scheduler;
    cpu_ = cpu;
    mem_ = mem;
    thread_ = std::jthread([this](std::stop_token stop) {
        render_loop(stop);
    });
    ready_.wait();
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
