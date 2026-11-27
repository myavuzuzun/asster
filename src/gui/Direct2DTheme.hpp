#pragma once

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#ifdef DrawText
#undef DrawText
#endif
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

namespace AssterUI {

struct D2DColor {
    float r, g, b, a;
    constexpr D2DColor(float red = 0.0f, float green = 0.0f, float blue = 0.0f, float alpha = 1.0f)
        : r(red), g(green), b(blue), a(alpha) {}
    D2D1_COLOR_F toD2D() const { return D2D1::ColorF(r, g, b, a); }
};

namespace Colors {
    static constexpr D2DColor BgChassis(0.065f, 0.070f, 0.080f);       // Matte Dark Slate #111214
    static constexpr D2DColor BgHeader(0.048f, 0.052f, 0.060f);        // Top Bar #0C0D0F
    static constexpr D2DColor BgCard(0.086f, 0.092f, 0.104f);          // Recessed Module Card #16171A
    static constexpr D2DColor BgInset(0.042f, 0.045f, 0.052f);         // Analyzer / Readout Well
    static constexpr D2DColor BorderChassis(0.18f, 0.19f, 0.22f);
    static constexpr D2DColor BorderCard(0.16f, 0.17f, 0.20f);
    static constexpr D2DColor GridLine(0.13f, 0.14f, 0.17f);

    static constexpr D2DColor TextActive(0.90f, 0.92f, 0.96f);
    static constexpr D2DColor TextBright(0.97f, 0.98f, 0.99f);
    static constexpr D2DColor TextMuted(0.52f, 0.55f, 0.61f);
    static constexpr D2DColor TextDim(0.34f, 0.36f, 0.41f);

    static constexpr D2DColor KnobTrack(0.18f, 0.19f, 0.23f);
    static constexpr D2DColor KnobArc(0.90f, 0.92f, 0.96f);
    static constexpr D2DColor KnobInner(0.11f, 0.12f, 0.14f);

    static constexpr D2DColor ButtonActiveBg(0.92f, 0.94f, 0.97f);
    static constexpr D2DColor ButtonActiveText(0.055f, 0.058f, 0.065f);
    static constexpr D2DColor ButtonInactiveBg(0.11f, 0.12f, 0.14f);
    static constexpr D2DColor ButtonHoverBg(0.16f, 0.17f, 0.20f);
    static constexpr D2DColor ButtonInactiveBorder(0.24f, 0.26f, 0.30f);

    static constexpr D2DColor AccentRed(0.92f, 0.28f, 0.28f);
    static constexpr D2DColor AccentAmber(0.92f, 0.68f, 0.24f);
    static constexpr D2DColor AccentCyan(0.42f, 0.78f, 0.94f);
    static constexpr D2DColor AccentGreen(0.30f, 0.85f, 0.52f);
    static constexpr D2DColor SpectrumFill(0.45f, 0.56f, 0.72f, 0.24f);
    static constexpr D2DColor PreSpectrumFill(0.35f, 0.37f, 0.42f, 0.16f);
    static constexpr D2DColor SpectrumLine(0.68f, 0.78f, 0.92f, 0.80f);
}

struct KnobWidget {
    int paramId;
    float x, y, radius;
    std::string title;
    std::wstring wTitle;
    const wchar_t* helpText = L"";
    bool isHero = false;
};

struct ButtonWidget {
    int paramId;
    int customAction = 0; // 0=param, 1=slotA, 2=slotB, 3=copyAtoB, 4=undo, 5=redo, 6=save, 7=load
    float x, y, w, h;
    std::string text;
    std::wstring wText;
    const wchar_t* helpText = L"";
    float targetValue;
    bool isToggle;
};

class UITheme {
public:
    static void initTextFormats(IDWriteFactory* pDWriteFactory,
                                IDWriteTextFormat** ppTitle,
                                IDWriteTextFormat** ppSubtitle,
                                IDWriteTextFormat** ppModule,
                                IDWriteTextFormat** ppKnobTitle,
                                IDWriteTextFormat** ppKnobVal,
                                IDWriteTextFormat** ppButton,
                                IDWriteTextFormat** ppButtonSub,
                                IDWriteTextFormat** ppSmall,
                                IDWriteTextFormat** ppMeterVal,
                                IDWriteTextFormat** ppMeterTag) {
        if (!pDWriteFactory) return;

        auto createFmt = [&](const wchar_t* font, float size, DWRITE_FONT_WEIGHT weight, IDWriteTextFormat** ppFmt, DWRITE_TEXT_ALIGNMENT align) {
            pDWriteFactory->CreateTextFormat(
                font, nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                size, L"en-us", ppFmt
            );
            if (*ppFmt) {
                (*ppFmt)->SetTextAlignment(align);
                (*ppFmt)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            }
        };

        createFmt(L"Segoe UI", 18.0f, DWRITE_FONT_WEIGHT_BOLD, ppTitle, DWRITE_TEXT_ALIGNMENT_LEADING);
        createFmt(L"Segoe UI", 9.0f, DWRITE_FONT_WEIGHT_NORMAL, ppSubtitle, DWRITE_TEXT_ALIGNMENT_LEADING);
        createFmt(L"Segoe UI", 10.0f, DWRITE_FONT_WEIGHT_BOLD, ppModule, DWRITE_TEXT_ALIGNMENT_LEADING);
        createFmt(L"Segoe UI", 8.0f, DWRITE_FONT_WEIGHT_BOLD, ppKnobTitle, DWRITE_TEXT_ALIGNMENT_CENTER);
        createFmt(L"Segoe UI", 7.8f, DWRITE_FONT_WEIGHT_NORMAL, ppKnobVal, DWRITE_TEXT_ALIGNMENT_CENTER);
        createFmt(L"Segoe UI", 8.0f, DWRITE_FONT_WEIGHT_BOLD, ppButton, DWRITE_TEXT_ALIGNMENT_CENTER);
        createFmt(L"Segoe UI", 7.2f, DWRITE_FONT_WEIGHT_NORMAL, ppButtonSub, DWRITE_TEXT_ALIGNMENT_CENTER);
        createFmt(L"Segoe UI", 7.2f, DWRITE_FONT_WEIGHT_NORMAL, ppSmall, DWRITE_TEXT_ALIGNMENT_LEADING);
        createFmt(L"Segoe UI", 12.5f, DWRITE_FONT_WEIGHT_BOLD, ppMeterVal, DWRITE_TEXT_ALIGNMENT_CENTER);
        createFmt(L"Segoe UI", 7.6f, DWRITE_FONT_WEIGHT_BOLD, ppMeterTag, DWRITE_TEXT_ALIGNMENT_CENTER);
    }
};

} // namespace AssterUI
#endif
