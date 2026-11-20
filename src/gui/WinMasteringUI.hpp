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

#include "Direct2DTheme.hpp"
#include "../dsp/AssterDSP.hpp"
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Asster {

struct PresetTab {
    int presetId;
    float x, y, w, h;
    const char* name;
    const char* targetLufs;
};

class WinMasteringUI {
public:
    static constexpr int UI_WIDTH = 960;
    static constexpr int UI_HEIGHT = 580;

    WinMasteringUI(AssterEngine& eng) : engine(eng) {
        initLayout();
        initDirect2D();
    }

    ~WinMasteringUI() {
        close();
        cleanupDirect2D();
    }

    bool open(HWND parent) {
        close();
        parentHwnd = parent;
        registerWindowClass();

        HINSTANCE hInstance = GetModuleHandle(nullptr);
        hwnd = CreateWindowExW(
            0,
            CLASS_NAME,
            L"Asster Mastering Tool",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, UI_WIDTH, UI_HEIGHT,
            parentHwnd,
            nullptr,
            hInstance,
            this
        );

        if (!hwnd) return false;

        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        createDeviceResources();
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        SetTimer(hwnd, TIMER_ID, 16, nullptr); // ~60 FPS smooth metering
        return true;
    }

    void close() {
        if (hwnd) {
            KillTimer(hwnd, TIMER_ID);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            DestroyWindow(hwnd);
            hwnd = nullptr;
        }
        discardDeviceResources();
    }

    void idle() {
        uint64_t now = GetTickCount64();
        if (now - lastIdleTick >= 16) {
            lastIdleTick = now;
            if (hwnd) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
    }

    void redraw() {
        lastIdleTick = GetTickCount64();
        if (hwnd) {
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }

    HWND getHwnd() const { return hwnd; }

private:
    static constexpr UINT_PTR TIMER_ID = 3001;
    static inline const wchar_t* CLASS_NAME = L"AssterMasteringD2DWindowClass";

    AssterEngine& engine;
    HWND parentHwnd = nullptr;
    HWND hwnd = nullptr;

    ID2D1Factory* pD2DFactory = nullptr;
    IDWriteFactory* pDWriteFactory = nullptr;
    ID2D1HwndRenderTarget* pRenderTarget = nullptr;
    ID2D1SolidColorBrush* pSolidBrush = nullptr;
    ID2D1StrokeStyle* pRoundStrokeStyle = nullptr;

    IDWriteTextFormat* pFormatTitle = nullptr;
    IDWriteTextFormat* pFormatSubtitle = nullptr;
    IDWriteTextFormat* pFormatModule = nullptr;
    IDWriteTextFormat* pFormatKnobTitle = nullptr;
    IDWriteTextFormat* pFormatKnobVal = nullptr;
    IDWriteTextFormat* pFormatButton = nullptr;
    IDWriteTextFormat* pFormatButtonSub = nullptr;
    IDWriteTextFormat* pFormatSmall = nullptr;
    IDWriteTextFormat* pFormatMeterVal = nullptr;
    IDWriteTextFormat* pFormatMeterTag = nullptr;

    std::vector<Fraudize::KnobWidget> knobs;
    std::vector<Fraudize::ButtonWidget> buttons;
    std::vector<PresetTab> presetTabs;

    int activeKnob = -1;
    int dragStartY = 0;
    float dragStartVal = 0.0f;
    uint64_t lastIdleTick = 0;
    std::mutex renderMutex;

    void registerWindowClass() {
        HINSTANCE hInstance = GetModuleHandle(nullptr);
        WNDCLASSEXW wc;
        if (GetClassInfoExW(hInstance, CLASS_NAME, &wc)) return;

        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProcStatic;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = CLASS_NAME;
        RegisterClassExW(&wc);
    }

    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        auto* pThis = reinterpret_cast<WinMasteringUI*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (uMsg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            pThis = reinterpret_cast<WinMasteringUI*>(cs->lpCreateParams);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
            return TRUE;
        }
        if (pThis) return pThis->handleMessage(hWnd, uMsg, wParam, lParam);
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    LRESULT handleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_PAINT: {
                PAINTSTRUCT ps;
                BeginPaint(hWnd, &ps);
                render();
                EndPaint(hWnd, &ps);
                return 0;
            }

            case WM_ERASEBKGND:
                return 1; // Direct2D handles full frame presentation

            case WM_TIMER:
                if (wParam == TIMER_ID) {
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;

            case WM_LBUTTONDOWN: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                SetCapture(hWnd);
                onMouseDown(mx, my, false);
                return 0;
            }

            case WM_LBUTTONDBLCLK: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                onMouseDown(mx, my, true);
                return 0;
            }

            case WM_MOUSEMOVE: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                bool shift = (wParam & MK_SHIFT) != 0;
                onMouseMove(mx, my, shift);
                return 0;
            }

            case WM_LBUTTONUP: {
                ReleaseCapture();
                activeKnob = -1;
                return 0;
            }

            case WM_MOUSEWHEEL: {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ScreenToClient(hWnd, &pt);
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                onMouseWheel(pt.x, pt.y, delta);
                return 0;
            }

            case WM_DESTROY:
                discardDeviceResources();
                hwnd = nullptr;
                return 0;

            default:
                break;
        }
        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    void onMouseDown(int mx, int my, bool isDoubleClick) {
        // 1. Preset Tabs
        for (const auto& tab : presetTabs) {
            if (mx >= tab.x && mx <= tab.x + tab.w && my >= tab.y && my <= tab.y + tab.h) {
                engine.loadPreset(tab.presetId);
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }

        // 2. Calibrate Level Button (x: 296, y: 264, w: 160, h: 28)
        if (mx >= 296 && mx <= 456 && my >= 264 && my <= 292) {
            engine.triggerCalibration();
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }

        // 3. Generic Buttons (Bypass, Sat Type, AutoTrim Mode)
        for (const auto& btn : buttons) {
            if (mx >= btn.x && mx <= btn.x + btn.w && my >= btn.y && my <= btn.y + btn.h) {
                if (btn.isToggle) {
                    float cur = engine.getParameter(btn.paramId);
                    engine.setParameter(btn.paramId, (cur >= 0.5f) ? 0.0f : 1.0f);
                } else {
                    engine.setParameter(btn.paramId, btn.targetValue);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }

        // 4. Knobs
        for (const auto& k : knobs) {
            float dx = mx - k.x;
            float dy = my - k.y;
            float hitRadius = k.radius + 8.0f;
            if (dx * dx + dy * dy <= hitRadius * hitRadius) {
                const auto* defs = getParameterDefs();
                if (isDoubleClick) {
                    engine.setParameter(k.paramId, defs[k.paramId].defaultVal);
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else {
                    activeKnob = k.paramId;
                    dragStartY = my;
                    dragStartVal = engine.getParameter(k.paramId);
                }
                return;
            }
        }
    }

    void onMouseMove(int mx, int my, bool isShift) {
        if (activeKnob >= 0) {
            const auto* defs = getParameterDefs();
            float range = defs[activeKnob].maxVal - defs[activeKnob].minVal;
            float sensitivity = isShift ? 0.001f : 0.0055f;
            float delta = (dragStartY - my) * range * sensitivity;
            float newVal = std::clamp(dragStartVal + delta, defs[activeKnob].minVal, defs[activeKnob].maxVal);
            if (defs[activeKnob].isInt) newVal = std::round(newVal);
            engine.setParameter(activeKnob, newVal);
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }

        // Hover cursor check
        bool overClickable = false;
        for (const auto& tab : presetTabs) {
            if (mx >= tab.x && mx <= tab.x + tab.w && my >= tab.y && my <= tab.y + tab.h) {
                overClickable = true; break;
            }
        }
        if (!overClickable && mx >= 296 && mx <= 456 && my >= 264 && my <= 292) {
            overClickable = true;
        }
        if (!overClickable) {
            for (const auto& btn : buttons) {
                if (mx >= btn.x && mx <= btn.x + btn.w && my >= btn.y && my <= btn.y + btn.h) {
                    overClickable = true; break;
                }
            }
        }
        if (!overClickable) {
            for (const auto& k : knobs) {
                float dx = mx - k.x;
                float dy = my - k.y;
                float hitRadius = k.radius + 8.0f;
                if (dx * dx + dy * dy <= hitRadius * hitRadius) {
                    overClickable = true; break;
                }
            }
        }
        SetCursor(LoadCursor(nullptr, overClickable ? IDC_HAND : IDC_ARROW));
    }

    void onMouseWheel(int mx, int my, int delta) {
        for (const auto& k : knobs) {
            float dx = mx - k.x;
            float dy = my - k.y;
            float hitRadius = k.radius + 8.0f;
            if (dx * dx + dy * dy <= hitRadius * hitRadius) {
                const auto* defs = getParameterDefs();
                float step = (delta > 0 ? 1.0f : -1.0f) * defs[k.paramId].step;
                float cur = engine.getParameter(k.paramId);
                float newVal = std::clamp(cur + step, defs[k.paramId].minVal, defs[k.paramId].maxVal);
                if (defs[k.paramId].isInt) newVal = std::round(newVal);
                engine.setParameter(k.paramId, newVal);
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            }
        }
    }

    void initDirect2D() {
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));

        if (pD2DFactory) {
            D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f
            );
            pD2DFactory->CreateStrokeStyle(&sp, nullptr, 0, &pRoundStrokeStyle);
        }

        Fraudize::UITheme::initTextFormats(
            pDWriteFactory, &pFormatTitle, &pFormatSubtitle, &pFormatModule,
            &pFormatKnobTitle, &pFormatKnobVal, &pFormatButton, &pFormatButtonSub,
            &pFormatSmall, &pFormatMeterVal, &pFormatMeterTag
        );
    }

    void cleanupDirect2D() {
        discardDeviceResources();
        if (pRoundStrokeStyle) { pRoundStrokeStyle->Release(); pRoundStrokeStyle = nullptr; }
        if (pFormatMeterTag)   { pFormatMeterTag->Release(); pFormatMeterTag = nullptr; }
        if (pFormatMeterVal)   { pFormatMeterVal->Release(); pFormatMeterVal = nullptr; }
        if (pFormatSmall)      { pFormatSmall->Release(); pFormatSmall = nullptr; }
        if (pFormatButtonSub)  { pFormatButtonSub->Release(); pFormatButtonSub = nullptr; }
        if (pFormatButton)     { pFormatButton->Release(); pFormatButton = nullptr; }
        if (pFormatKnobVal)    { pFormatKnobVal->Release(); pFormatKnobVal = nullptr; }
        if (pFormatKnobTitle)  { pFormatKnobTitle->Release(); pFormatKnobTitle = nullptr; }
        if (pFormatModule)     { pFormatModule->Release(); pFormatModule = nullptr; }
        if (pFormatSubtitle)   { pFormatSubtitle->Release(); pFormatSubtitle = nullptr; }
        if (pFormatTitle)      { pFormatTitle->Release(); pFormatTitle = nullptr; }
        if (pDWriteFactory)    { pDWriteFactory->Release(); pDWriteFactory = nullptr; }
        if (pD2DFactory)       { pD2DFactory->Release(); pD2DFactory = nullptr; }
    }

    void createDeviceResources() {
        if (!pRenderTarget && hwnd && pD2DFactory) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            D2D1_SIZE_U size = D2D1::SizeU(std::max<UINT32>(1, rc.right - rc.left), std::max<UINT32>(1, rc.bottom - rc.top));

            D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            );
            D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hwnd, size, D2D1_PRESENT_OPTIONS_IMMEDIATELY);

            if (SUCCEEDED(pD2DFactory->CreateHwndRenderTarget(rtProps, hwndProps, &pRenderTarget))) {
                pRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                pRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
                pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &pSolidBrush);
            }
        }
    }

    void discardDeviceResources() {
        if (pSolidBrush)    { pSolidBrush->Release(); pSolidBrush = nullptr; }
        if (pRenderTarget)  { pRenderTarget->Release(); pRenderTarget = nullptr; }
    }

    void initLayout() {
        knobs.clear();
        buttons.clear();
        presetTabs.clear();

        auto addKnob = [&](int id, float x, float y, float r, const char* title, bool hero = false) {
            std::string s(title);
            std::wstring ws(s.begin(), s.end());
            knobs.push_back({ id, x, y, r, s, ws, hero });
        };

        auto addButton = [&](int id, float x, float y, float w, float h, const char* text, float targetVal, bool isToggle) {
            std::string s(text);
            std::wstring ws(s.begin(), s.end());
            buttons.push_back({ id, x, y, w, h, s, ws, targetVal, isToggle });
        };

        // 1. Header Bypass Toggle Button (Top Right)
        addButton(PARAM_BYPASS, 746, 12, 96, 24, "MASTER ON", 1.0f, true);

        // 2. 7 Metal Presets Ribbon (y = 56..88)
        float startX = 16.0f;
        float tabW = 126.0f;
        float tabH = 32.0f;
        float spacing = 7.0f;

        presetTabs.push_back({ 0, startX + 0 * (tabW + spacing), 56.0f, tabW, tabH, "EXTREME", "-8.0 LUFS" });
        presetTabs.push_back({ 1, startX + 1 * (tabW + spacing), 56.0f, tabW, tabH, "DOOM", "-10.5 LUFS" });
        presetTabs.push_back({ 2, startX + 2 * (tabW + spacing), 56.0f, tabW, tabH, "SLUDGE", "-9.0 LUFS" });
        presetTabs.push_back({ 3, startX + 3 * (tabW + spacing), 56.0f, tabW, tabH, "GROOVE", "-8.5 LUFS" });
        presetTabs.push_back({ 4, startX + 4 * (tabW + spacing), 56.0f, tabW, tabH, "DJENT", "-7.5 LUFS" });
        presetTabs.push_back({ 5, startX + 5 * (tabW + spacing), 56.0f, tabW, tabH, "CLASSIC", "-11.0 LUFS" });
        presetTabs.push_back({ 6, startX + 6 * (tabW + spacing), 56.0f, tabW + 2.0f, tabH, "TRANSPARENT", "-14.0 LUFS" });

        // Card 1: TONAL SCULPT (EQ) (x: 16, y: 96, w: 220, h: 342)
        addKnob(PARAM_EQ_BASS, 71,  162, 21, "BASS PUNCH");
        addKnob(PARAM_EQ_MUD,  177, 162, 21, "MUD SCOOP");
        addKnob(PARAM_EQ_BITE, 71,  250, 21, "BITE ATTACK");
        addKnob(PARAM_EQ_AIR,  177, 250, 21, "AIR SHEEN");
        addKnob(PARAM_SUB_CUT, 124, 344, 21, "SUB CUT (HPF)");

        // Card 2: MASTER DYNAMICS & PUSH (Hero Centerpiece) (x: 244, y: 96, w: 264, h: 342)
        addKnob(PARAM_MASTER_PUSH, 376, 178, 48, "MASTER PUSH", true);
        addKnob(PARAM_CEILING,     376, 370, 21, "CEILING (dBTP)");

        // Card 3: ANALOG COLOR & GLUE (x: 516, y: 96, w: 216, h: 342)
        addKnob(PARAM_SAT_DRIVE,     570, 162, 21, "TAPE DRIVE");
        addKnob(PARAM_SAT_MIX,       674, 162, 21, "SAT MIX");
        addKnob(PARAM_GLUE_AMOUNT,   570, 252, 21, "BUS GLUE");
        addKnob(PARAM_CLIPPER_DRIVE, 674, 252, 21, "CLIP DRIVE");

        // Card 3: Saturation Type Buttons (Tape=0, Tube=1, Solid=2)
        addButton(PARAM_SAT_TYPE, 532, 344, 56, 22, "TAPE",  0.0f, false);
        addButton(PARAM_SAT_TYPE, 596, 344, 56, 22, "TUBE",  1.0f, false);
        addButton(PARAM_SAT_TYPE, 660, 344, 56, 22, "SOLID", 2.0f, false);

        // Card 4: STEREO & TRIM (x: 740, y: 96, w: 204, h: 342)
        addKnob(PARAM_STEREO_WIDTH, 792, 162, 21, "STEREO WIDTH");
        addKnob(PARAM_MONO_FREQ,    892, 162, 21, "MONO BASS");
        addKnob(PARAM_IN_TRIM,      842, 252, 21, "INPUT TRIM");

        // Card 4: AutoTrim Mode Buttons (Off=0, Learn=1, Cont=2)
        addButton(PARAM_AUTOTRIM_MODE, 752, 344, 56, 22, "OFF",   0.0f, false);
        addButton(PARAM_AUTOTRIM_MODE, 814, 344, 56, 22, "LEARN", 1.0f, false);
        addButton(PARAM_AUTOTRIM_MODE, 876, 344, 56, 22, "CONT",  2.0f, false);
    }

    void render() {
        std::lock_guard<std::mutex> lock(renderMutex);
        if (!pRenderTarget) {
            createDeviceResources();
            if (!pRenderTarget) return;
        }

        pRenderTarget->BeginDraw();

        // 1. Background Chassis
        setBrush(Fraudize::Colors::BgChassis);
        pRenderTarget->FillRectangle(D2D1::RectF(0, 0, UI_WIDTH, UI_HEIGHT), pSolidBrush);

        setBrush(Fraudize::Colors::BorderChassis);
        pRenderTarget->DrawRectangle(D2D1::RectF(0.5f, 0.5f, UI_WIDTH - 0.5f, UI_HEIGHT - 0.5f), pSolidBrush, 1.0f);

        // 2. Header Bar (0 to 48)
        drawHeader();

        // 3. Preset Ribbon (56 to 88)
        drawPresetRibbon();

        // 4. Modular Cards
        drawModuleCard(16,  96, 220, 342, L"TONAL SCULPT (4-BAND EQ)");
        drawModuleCard(244, 96, 264, 342, L"MASTER PUSH & DYNAMICS");
        drawModuleCard(516, 96, 216, 342, L"ANALOG COLOR & GLUE");
        drawModuleCard(740, 96, 204, 342, L"STEREO & TRIM");

        // Subheaders inside Cards 3 & 4
        setBrush(Fraudize::Colors::TextMuted);
        drawText(L"SATURATOR TYPE", 532, 324, 180, 16, pFormatKnobTitle);
        drawText(L"AUTOTRIM MODE",  752, 324, 180, 16, pFormatKnobTitle);

        // 5. Draw Buttons
        for (const auto& btn : buttons) {
            drawButton(btn);
        }

        // 6. Draw Knobs
        for (const auto& k : knobs) {
            drawKnob(k);
        }

        // 7. Calibrate Module in Card 2
        drawCalibrateModule();

        // 8. Precision Metering Deck (Bottom Panel: 446 to 568)
        drawMeteringDeck();

        HRESULT hr = pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            discardDeviceResources();
        }
    }

    void drawHeader() {
        setBrush(Fraudize::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(0, 0, UI_WIDTH, 48), pSolidBrush);
        setBrush(Fraudize::Colors::BorderCard);
        pRenderTarget->DrawLine(D2D1::Point2F(0, 48), D2D1::Point2F(UI_WIDTH, 48), pSolidBrush, 1.0f);

        // Header Title
        setBrush(Fraudize::Colors::TextBright);
        drawText(L"ASSTER", 18, 10, 190, 22, pFormatTitle);

        // Subtitle
        setBrush(Fraudize::Colors::TextMuted);
        drawText(L"BY MYAVUZUZUN", 18, 31, 200, 14, pFormatSubtitle);

        // Top Right Format Badge
        setBrush(Fraudize::Colors::BgCard);
        pRenderTarget->FillRectangle(D2D1::RectF(852, 12, 944, 36), pSolidBrush);
        setBrush(Fraudize::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(852.5f, 12.5f, 943.5f, 35.5f), pSolidBrush, 1.0f);

        setBrush(Fraudize::Colors::BadgeAudio);
        drawText(L"MASTERING FX", 852, 12, 92, 24, pFormatButton);
    }

    void drawPresetRibbon() {
        int curPreset = engine.getCurrentPreset();

        for (const auto& tab : presetTabs) {
            bool active = (tab.presetId == curPreset);
            D2D1_RECT_F r = D2D1::RectF(tab.x, tab.y, tab.x + tab.w, tab.y + tab.h);

            std::wstring wName(tab.name, tab.name + strlen(tab.name));
            std::wstring wTag(tab.targetLufs, tab.targetLufs + strlen(tab.targetLufs));

            if (active) {
                // Inverted High Contrast White Pill
                setBrush(Fraudize::Colors::ButtonActiveBg);
                pRenderTarget->FillRectangle(r, pSolidBrush);

                setBrush(Fraudize::Colors::ButtonActiveText);
                drawText(wName.c_str(), tab.x, tab.y + 2.0f, tab.w, 15.0f, pFormatButton);
                drawText(wTag.c_str(),  tab.x, tab.y + 16.0f, tab.w, 13.0f, pFormatButtonSub);
            } else {
                // Inactive Dark Slate Pill with Border
                setBrush(Fraudize::Colors::ButtonInactiveBg);
                pRenderTarget->FillRectangle(r, pSolidBrush);
                setBrush(Fraudize::Colors::ButtonInactiveBorder);
                pRenderTarget->DrawRectangle(r, pSolidBrush, 1.0f);

                setBrush(Fraudize::Colors::TextActive);
                drawText(wName.c_str(), tab.x, tab.y + 2.0f, tab.w, 15.0f, pFormatButton);

                setBrush(Fraudize::Colors::TextDim);
                drawText(wTag.c_str(),  tab.x, tab.y + 16.0f, tab.w, 13.0f, pFormatButtonSub);
            }
        }
    }

    void drawModuleCard(float x, float y, float w, float h, const wchar_t* title) {
        setBrush(Fraudize::Colors::BgCard);
        pRenderTarget->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pSolidBrush);

        setBrush(Fraudize::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(x + 0.5f, y + 0.5f, x + w - 0.5f, y + h - 0.5f), pSolidBrush, 1.0f);

        setBrush(Fraudize::Colors::TextActive);
        drawText(title, x + 12, y + 8, w - 24, 20, pFormatModule);
    }

    void drawButton(const Fraudize::ButtonWidget& btn) {
        float cur = engine.getParameter(btn.paramId);
        bool active = false;

        if (btn.isToggle) {
            active = (cur >= 0.5f);
        } else {
            active = (std::abs(cur - btn.targetValue) < 0.25f);
        }

        D2D1_RECT_F r = D2D1::RectF(btn.x, btn.y, btn.x + btn.w, btn.y + btn.h);

        // Special handling for Header Bypass Button
        if (btn.paramId == PARAM_BYPASS) {
            if (active) { // Bypassed state
                setBrush(Fraudize::Colors::ButtonActiveBg);
                pRenderTarget->FillRectangle(r, pSolidBrush);
                setBrush(Fraudize::Colors::ButtonActiveText);
                drawText(L"BYPASSED", btn.x, btn.y, btn.w, btn.h, pFormatButton);
            } else { // Master Active state
                setBrush(Fraudize::Colors::ButtonInactiveBg);
                pRenderTarget->FillRectangle(r, pSolidBrush);
                setBrush(Fraudize::Colors::ButtonInactiveBorder);
                pRenderTarget->DrawRectangle(r, pSolidBrush, 1.0f);

                // Solid White Indicator Dot
                setBrush(Fraudize::Colors::TextBright);
                pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(btn.x + 12.0f, btn.y + btn.h * 0.5f), 3.5f, 3.5f), pSolidBrush);

                setBrush(Fraudize::Colors::TextActive);
                drawText(L"MASTER ON", btn.x + 8.0f, btn.y, btn.w - 8.0f, btn.h, pFormatButton);
            }
            return;
        }

        // Standard Button Style
        if (active) {
            setBrush(Fraudize::Colors::ButtonActiveBg);
            pRenderTarget->FillRectangle(r, pSolidBrush);
            setBrush(Fraudize::Colors::ButtonActiveText);
            drawText(btn.wText.c_str(), btn.x, btn.y, btn.w, btn.h, pFormatButton);
        } else {
            setBrush(Fraudize::Colors::ButtonInactiveBg);
            pRenderTarget->FillRectangle(r, pSolidBrush);
            setBrush(Fraudize::Colors::ButtonInactiveBorder);
            pRenderTarget->DrawRectangle(r, pSolidBrush, 1.0f);
            setBrush(Fraudize::Colors::TextDim);
            drawText(btn.wText.c_str(), btn.x, btn.y, btn.w, btn.h, pFormatButton);
        }
    }

    void drawKnob(const Fraudize::KnobWidget& k) {
        float val = engine.getParameter(k.paramId);
        const auto* defs = getParameterDefs();
        float norm = (val - defs[k.paramId].minVal) / (defs[k.paramId].maxVal - defs[k.paramId].minVal);
        norm = std::clamp(norm, 0.0f, 1.0f);

        float startAngle = 135.0f;
        float totalSweep = 270.0f;
        float sweepAngle = norm * totalSweep;

        float radStart = startAngle * 3.14159265f / 180.0f;
        float radEnd   = (startAngle + sweepAngle) * 3.14159265f / 180.0f;
        float radMax   = (startAngle + totalSweep) * 3.14159265f / 180.0f;

        float trackStroke = k.isHero ? 3.5f : 2.5f;
        float activeStroke = k.isHero ? 4.5f : 2.5f;

        // 1. Dark Knob Track
        drawArc(k.x, k.y, k.radius, radStart, radMax, Fraudize::Colors::KnobTrack, trackStroke);

        // 2. Active Silver Arc
        if (sweepAngle > 0.001f) {
            drawArc(k.x, k.y, k.radius, radStart, radEnd, Fraudize::Colors::KnobArc, activeStroke);
        }

        // 3. Center Rotor Cap
        float innerR = k.radius - (k.isHero ? 6.5f : 4.5f);
        setBrush(Fraudize::Colors::KnobInner);
        pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(k.x, k.y), innerR, innerR), pSolidBrush);
        setBrush(Fraudize::Colors::BorderCard);
        pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(k.x, k.y), innerR, innerR), pSolidBrush, 1.0f);

        // 4. Indicator Line (Notch)
        float p1x = k.x + (innerR * 0.35f) * std::cos(radEnd);
        float p1y = k.y + (innerR * 0.35f) * std::sin(radEnd);
        float p2x = k.x + (innerR * 0.90f) * std::cos(radEnd);
        float p2y = k.y + (innerR * 0.90f) * std::sin(radEnd);

        setBrush(Fraudize::Colors::TextBright);
        pRenderTarget->DrawLine(D2D1::Point2F(p1x, p1y), D2D1::Point2F(p2x, p2y), pSolidBrush, k.isHero ? 2.5f : 1.8f, pRoundStrokeStyle);

        // 5. Title Text below knob
        float titleY = k.y + k.radius + (k.isHero ? 12.0f : 8.0f);
        setBrush(Fraudize::Colors::TextMuted);
        drawText(k.wTitle.c_str(), k.x - k.radius - 24, titleY, (k.radius + 24) * 2, 14, pFormatKnobTitle);

        // 6. Recessed Numeric Value Readout Box
        char valStr[32];
        if (defs[k.paramId].isInt) {
            snprintf(valStr, sizeof(valStr), "%d %s", static_cast<int>(std::round(val)), defs[k.paramId].label);
        } else if (std::abs(val) < 10.0f) {
            snprintf(valStr, sizeof(valStr), "%+.1f %s", val, defs[k.paramId].label);
        } else {
            snprintf(valStr, sizeof(valStr), "%+.0f %s", val, defs[k.paramId].label);
        }
        std::wstring wVal(valStr, valStr + strlen(valStr));

        float boxW = k.isHero ? 80.0f : 56.0f;
        float boxH = k.isHero ? 18.0f : 15.0f;
        float boxX = k.x - boxW * 0.5f;
        float boxY = titleY + (k.isHero ? 16.0f : 14.0f);

        setBrush(Fraudize::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(boxX, boxY, boxX + boxW, boxY + boxH), pSolidBrush);
        setBrush(Fraudize::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(boxX + 0.5f, boxY + 0.5f, boxX + boxW - 0.5f, boxY + boxH - 0.5f), pSolidBrush, 1.0f);

        setBrush(Fraudize::Colors::TextActive);
        drawText(wVal.c_str(), boxX, boxY, boxW, boxH, pFormatKnobVal);
    }

    void drawCalibrateModule() {
        float x = 296.0f, y = 264.0f, w = 160.0f, h = 28.0f;
        bool isLearning = engine.isCalibrating();

        D2D1_RECT_F r = D2D1::RectF(x, y, x + w, y + h);

        if (isLearning) {
            setBrush(Fraudize::Colors::ButtonActiveBg);
            pRenderTarget->FillRectangle(r, pSolidBrush);
            setBrush(Fraudize::Colors::ButtonActiveText);
            drawText(L"CALIBRATING...", x, y, w, h, pFormatButton);
        } else {
            setBrush(Fraudize::Colors::ButtonInactiveBg);
            pRenderTarget->FillRectangle(r, pSolidBrush);
            setBrush(Fraudize::Colors::ButtonInactiveBorder);
            pRenderTarget->DrawRectangle(r, pSolidBrush, 1.0f);
            setBrush(Fraudize::Colors::TextBright);
            drawText(L"CALIBRATE LEVEL", x, y, w, h, pFormatButton);
        }

        // Inset AutoTrim Readout Box
        float rx = 266.0f, ry = 298.0f, rw = 220.0f, rh = 22.0f;
        setBrush(Fraudize::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(rx, ry, rx + rw, ry + rh), pSolidBrush);
        setBrush(Fraudize::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(rx + 0.5f, ry + 0.5f, rx + rw - 0.5f, ry + rh - 0.5f), pSolidBrush, 1.0f);

        char gainBuf[64];
        snprintf(gainBuf, sizeof(gainBuf), "AutoTrim: %+0.1f dB  |  In: %0.1f LUFS",
                 engine.getAutoTrimGainDb(), engine.getIncomingLUFS());
        std::wstring wGain(gainBuf, gainBuf + strlen(gainBuf));

        setBrush(Fraudize::Colors::TextActive);
        drawText(wGain.c_str(), rx, ry, rw, rh, pFormatKnobVal);
    }

    void drawMeteringDeck() {
        float deckX = 16.0f;
        float deckY = 446.0f;
        float deckW = 928.0f;
        float deckH = 122.0f;

        // Card Chassis
        setBrush(Fraudize::Colors::BgCard);
        pRenderTarget->FillRectangle(D2D1::RectF(deckX, deckY, deckX + deckW, deckY + deckH), pSolidBrush);
        setBrush(Fraudize::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(deckX + 0.5f, deckY + 0.5f, deckX + deckW - 0.5f, deckY + deckH - 0.5f), pSolidBrush, 1.0f);

        // Header Title
        setBrush(Fraudize::Colors::TextActive);
        drawText(L"PRECISION METERING DECK", deckX + 12.0f, deckY + 8.0f, 240.0f, 18.0f, pFormatModule);

        float curLUFS = engine.getOutputShortTermLUFS();
        float tpDb    = engine.getOutputTruePeakdBTP();
        float glueGR  = engine.getGlueGR();
        float limGR   = engine.getLimiterGR();

        // 4 Inset Precision Metric Boxes
        struct MetricBox {
            float x;
            const wchar_t* tag;
            wchar_t val[32];
        };

        MetricBox boxes[4];
        boxes[0].x = deckX + 12.0f;
        boxes[0].tag = L"LOUDNESS";
        swprintf_s(boxes[0].val, 32, L"%0.1f LUFS", curLUFS);

        boxes[1].x = deckX + 152.0f;
        boxes[1].tag = L"TRUE PEAK";
        swprintf_s(boxes[1].val, 32, L"%0.2f dBTP", tpDb);

        boxes[2].x = deckX + 292.0f;
        boxes[2].tag = L"GLUE GR";
        swprintf_s(boxes[2].val, 32, L"-%0.1f dB", glueGR);

        boxes[3].x = deckX + 432.0f;
        boxes[3].tag = L"LIMITER GR";
        swprintf_s(boxes[3].val, 32, L"-%0.1f dB", limGR);

        for (int i = 0; i < 4; ++i) {
            float bx = boxes[i].x;
            float by = deckY + 30.0f;
            float bw = 130.0f;
            float bh = 42.0f;

            setBrush(Fraudize::Colors::BgHeader);
            pRenderTarget->FillRectangle(D2D1::RectF(bx, by, bx + bw, by + bh), pSolidBrush);
            setBrush(Fraudize::Colors::BorderCard);
            pRenderTarget->DrawRectangle(D2D1::RectF(bx + 0.5f, by + 0.5f, bx + bw - 0.5f, by + bh - 0.5f), pSolidBrush, 1.0f);

            setBrush(Fraudize::Colors::TextMuted);
            drawText(boxes[i].tag, bx, by + 3.0f, bw, 13.0f, pFormatMeterTag);

            setBrush(Fraudize::Colors::TextBright);
            drawText(boxes[i].val, bx, by + 16.0f, bw, 22.0f, pFormatMeterVal);
        }

        // ZERO-CLIP PROTECTED Badge
        float badgeX = deckX + 712.0f;
        float badgeY = deckY + 34.0f;
        float badgeW = 204.0f;
        float badgeH = 34.0f;

        setBrush(Fraudize::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(badgeX, badgeY, badgeX + badgeW, badgeY + badgeH), pSolidBrush);
        setBrush(Fraudize::Colors::TextActive);
        pRenderTarget->DrawRectangle(D2D1::RectF(badgeX + 0.5f, badgeY + 0.5f, badgeX + badgeW - 0.5f, badgeY + badgeH - 0.5f), pSolidBrush, 1.0f);

        // Glowing Solid White Indicator Dot
        setBrush(Fraudize::Colors::TextBright);
        pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(badgeX + 16.0f, badgeY + badgeH * 0.5f), 4.0f, 4.0f), pSolidBrush);

        setBrush(Fraudize::Colors::TextBright);
        drawText(L"ZERO-CLIP PROTECTED", badgeX + 26.0f, badgeY, badgeW - 26.0f, badgeH, pFormatButton);

        // Horizontal Gain Reduction Meter
        float meterX = deckX + 12.0f;
        float meterY = deckY + 84.0f;
        float meterW = 550.0f;
        float meterH = 12.0f;

        setBrush(Fraudize::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(meterX, meterY, meterX + meterW, meterY + meterH), pSolidBrush);
        setBrush(Fraudize::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(meterX + 0.5f, meterY + 0.5f, meterX + meterW - 0.5f, meterY + meterH - 0.5f), pSolidBrush, 1.0f);

        float totalGR = limGR + glueGR;
        float grNorm = std::clamp(totalGR / 12.0f, 0.0f, 1.0f);
        float fillW = grNorm * meterW;

        if (fillW > 1.0f) {
            setBrush(Fraudize::Colors::KnobArc);
            pRenderTarget->FillRectangle(D2D1::RectF(meterX, meterY, meterX + fillW, meterY + meterH), pSolidBrush);
        }

        // Calibrated Ticks
        const wchar_t* ticks[5] = { L"0 dB", L"-3 dB", L"-6 dB", L"-9 dB", L"-12 dB" };
        setBrush(Fraudize::Colors::TextMuted);
        for (int t = 0; t < 5; ++t) {
            float tx = meterX + (meterW * 0.25f * t);
            drawText(ticks[t], tx - 22.0f, meterY + 14.0f, 44.0f, 12.0f, pFormatSmall);
        }

        // GR Label
        drawText(L"TOTAL GAIN REDUCTION", meterX + meterW + 16.0f, meterY - 1.0f, 160.0f, 14.0f, pFormatKnobTitle);
    }

    void drawArc(float cx, float cy, float r, float startAngleRad, float endAngleRad, const Fraudize::D2DColor& color, float strokeWidth) {
        if (!pD2DFactory || !pRenderTarget) return;
        if (std::abs(endAngleRad - startAngleRad) < 0.0001f) return;

        ID2D1PathGeometry* pPath = nullptr;
        if (FAILED(pD2DFactory->CreatePathGeometry(&pPath))) return;

        ID2D1GeometrySink* pSink = nullptr;
        if (SUCCEEDED(pPath->Open(&pSink))) {
            D2D1_POINT_2F p0 = D2D1::Point2F(cx + r * std::cos(startAngleRad), cy + r * std::sin(startAngleRad));
            D2D1_POINT_2F p1 = D2D1::Point2F(cx + r * std::cos(endAngleRad), cy + r * std::sin(endAngleRad));

            pSink->BeginFigure(p0, D2D1_FIGURE_BEGIN_HOLLOW);
            float angleDiff = endAngleRad - startAngleRad;
            pSink->AddArc(D2D1::ArcSegment(
                p1, D2D1::SizeF(r, r), 0.0f,
                D2D1_SWEEP_DIRECTION_CLOCKWISE,
                (angleDiff > 3.14159265f) ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
            ));
            pSink->EndFigure(D2D1_FIGURE_END_OPEN);
            pSink->Close();
            pSink->Release();

            setBrush(color);
            pRenderTarget->DrawGeometry(pPath, pSolidBrush, strokeWidth, pRoundStrokeStyle);
        }
        pPath->Release();
    }

    void drawText(const wchar_t* str, float x, float y, float w, float h, IDWriteTextFormat* pFmt) {
        if (pRenderTarget && pSolidBrush && pFmt && str) {
            pRenderTarget->DrawText(str, static_cast<UINT32>(wcslen(str)), pFmt, D2D1::RectF(x, y, x + w, y + h), pSolidBrush);
        }
    }

    void setBrush(const Fraudize::D2DColor& c) {
        if (pSolidBrush) pSolidBrush->SetColor(c.toD2D());
    }
};

} // namespace Asster
#endif
