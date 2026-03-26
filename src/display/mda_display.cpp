// MdaRasterizer -- 80x25 MDA text rasterization via Direct2D/DirectWrite.

#include "display/mda_display.h"
#include "isa/isa_card.h"
#include "isa/isa_mda.h"

#include <cmath>

using Microsoft::WRL::ComPtr;

namespace bench {

static constexpr int MDA_COLS = 80;
static constexpr int MDA_ROWS = 25;

// ========================================================================
// CP437 -> Unicode
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
// MdaRasterizer
// ========================================================================

const ISA_Card* MdaRasterizer::card() const {
    return mda_card_;
}

bool MdaRasterizer::init(const RenderContext& rc) {
    cellW_ = rc.cellW;
    cellH_ = rc.cellH;

    // DirectWrite font setup
    ComPtr<IDWriteFontFile> fontFile;
    rc.dwrite->CreateFontFileReference(L"assets/Ac437_IBM_MDA.ttf", nullptr, &fontFile);
    ComPtr<IDWriteFontSetBuilder1> fontSetBuilder;
    rc.dwrite->CreateFontSetBuilder(&fontSetBuilder);
    fontSetBuilder->AddFontFile(fontFile.Get());
    ComPtr<IDWriteFontSet> fontSet;
    fontSetBuilder->CreateFontSet(&fontSet);
    ComPtr<IDWriteFontCollection1> fc1;
    rc.dwrite->CreateFontCollectionFromFontSet(fontSet.Get(), &fc1);

    rc.dwrite->CreateTextFormat(L"Ac437 IBM MDA", fc1.Get(),
        DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        rc.cellH, L"en-us", &textFormat_);
    textFormat_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    textFormat_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    textFormat_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

    // D2D brushes
    rc.d2d_ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.85f, 0.0f), &greenBrush_);
    rc.d2d_ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 1.0f, 0.0f), &brightGreenBrush_);
    rc.d2d_ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f), &blackBrush_);
    rc.d2d_ctx->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.65f, 0.0f), &underlineBrush_);

    // QPC for blink timing (frame-rate independent).
    QueryPerformanceFrequency(&qpc_freq_);
    QueryPerformanceCounter(&qpc_start_);

    return true;
}

void MdaRasterizer::render(const RenderContext& rc) {
    auto* d2dCtx = rc.d2d_ctx;
    const uint8_t* vram = vram_;
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

    if (mda_card_) {
        const uint8_t* cr = mda_card_->crtc_regs();
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

                float x  = floorf(col * cellW_);
                float y  = floorf(row * cellH_);
                float x2 = floorf((col + 1) * cellW_);
                float y2 = floorf((row + 1) * cellH_);
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
                        d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), greenBrush_.Get());
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
                    d2dCtx->FillRectangle(D2D1::RectF(x, y, x2, y2), greenBrush_.Get());

                    // Draw character in black.
                    if (ch != 0x20) {
                        wchar_t wc = cp437_to_unicode(ch);
                        d2dCtx->DrawText(&wc, 1, textFormat_.Get(),
                            D2D1::RectF(x, y, x2, y2), blackBrush_.Get());
                    }
                    goto draw_cursor;
                }

                {
                    // Normal or underline.
                    ID2D1SolidColorBrush* brush = intensity ? brightGreenBrush_.Get() : greenBrush_.Get();

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
                        d2dCtx->DrawText(&wc, 1, textFormat_.Get(),
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
                    d2dCtx->FillRectangle(D2D1::RectF(x, cy1, x2, cy2), greenBrush_.Get());
                }
            }
        }
    }

    d2dCtx->EndDraw();
}

} // namespace bench
