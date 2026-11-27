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
#include <commdlg.h>
#ifdef DrawText
#undef DrawText
#endif

#include "Direct2DTheme.hpp"
#include "SpectrumFFT.hpp"
#include "ReferenceAnalyzer.hpp"
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
    static constexpr int UI_WIDTH = 1120;
    static constexpr int UI_HEIGHT = 720;

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
            L"Mastering Processor",
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

        SetTimer(hwnd, TIMER_ID, 33, nullptr); // ~30 FPS light metering refresh
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
        if (now - lastIdleTick >= 33) {
            lastIdleTick = now;
            if (hwnd && shouldAnimate()) InvalidateRect(hwnd, nullptr, FALSE);
        }
    }

    void redraw() {
        eqCurveDirty = true;
        quietFrames = 0;
        lastIdleTick = GetTickCount64();
        if (hwnd) InvalidateRect(hwnd, nullptr, FALSE);
    }

    HWND getHwnd() const { return hwnd; }

private:
    static constexpr UINT_PTR TIMER_ID = 3001;
    static inline const wchar_t* CLASS_NAME = L"MasteringProcessorD2DWindowClass";

    static constexpr float SPEC_X = 16.0f;
    static constexpr float SPEC_Y = 84.0f;
    static constexpr float SPEC_W = 690.0f;
    static constexpr float SPEC_H = 150.0f;

    AssterEngine& engine;
    HWND parentHwnd = nullptr;
    HWND hwnd = nullptr;

    ID2D1Factory* pD2DFactory = nullptr;
    IDWriteFactory* pDWriteFactory = nullptr;
    ID2D1HwndRenderTarget* pRenderTarget = nullptr;
    ID2D1SolidColorBrush* pSolidBrush = nullptr;
    ID2D1StrokeStyle* pRoundStrokeStyle = nullptr;
    ID2D1StrokeStyle* pDashedStrokeStyle = nullptr;

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

    std::vector<AssterUI::KnobWidget> knobs;
    std::vector<AssterUI::ButtonWidget> buttons;
    std::vector<PresetTab> presetTabs;

    AssterUI::SpectrumFFT postFftProcessor;
    AssterUI::SpectrumFFT preFftProcessor;
    AssterUI::ReferenceAnalyzer refAnalyzer;
    float fftSamples[LoudnessMeter::FFT_SIZE] = {0.0f};
    float preFftSamples[LoudnessMeter::FFT_SIZE] = {0.0f};
    float fftBinsDb[128] = {0.0f};
    float preFftBinsDb[128] = {0.0f};
    float scopeL[LoudnessMeter::SCOPE_SIZE] = {0.0f};
    float scopeR[LoudnessMeter::SCOPE_SIZE] = {0.0f};
    LoudnessMeter::HistoryPoint historyPts[LoudnessMeter::HISTORY_SIZE];

    int activeKnob = -1;
    int activeEqNode = -1;
    int hoverBtnIdx = -1;
    int dragStartY = 0;
    float dragStartVal = 0.0f;
    bool eqCurveDirty = true;
    float cachedEqY[96] = {0.0f};
    bool isDefaultHelp = true;
    const wchar_t* hoverHelp = L"Hover any knob, button, or EQ node for help. Lower INTENSITY (0-35%) for transparent, uncolored mastering.";
    uint64_t lastIdleTick = 0;
    int quietFrames = 0;
    std::mutex renderMutex;

    bool shouldAnimate() {
        bool active = (activeKnob >= 0) || (activeEqNode >= 0) ||
                      (engine.getOutputTruePeakdBTP() > -85.0f) ||
                      engine.isCalibrating() || engine.isAutoMastering();
        if (active) {
            quietFrames = 0;
            return true;
        }
        if (quietFrames < 15) {
            ++quietFrames;
            return true;
        }
        return false;
    }

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
                return 1;
            case WM_TIMER:
                if (wParam == TIMER_ID && shouldAnimate()) InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case WM_KEYDOWN: {
                bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (ctrl && wParam == 'Z') {
                    if (engine.undo()) InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (ctrl && wParam == 'Y') {
                    if (engine.redo()) InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                break;
            }
            case WM_LBUTTONDOWN: {
                SetFocus(hWnd);
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                SetCapture(hWnd);
                onMouseDown(mx, my, false);
                return 0;
            }
            case WM_RBUTTONDOWN: {
                int mx = GET_X_LPARAM(lParam);
                int my = GET_Y_LPARAM(lParam);
                onRightClick(mx, my);
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
            case WM_LBUTTONUP:
                ReleaseCapture();
                activeKnob = -1;
                activeEqNode = -1;
                return 0;
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

    static float freqToSpecX(float freqHz) {
        float logMin = std::log10(20.0f);
        float logMax = std::log10(20000.0f);
        float t = std::clamp((std::log10(std::clamp(freqHz, 20.0f, 20000.0f)) - logMin) / (logMax - logMin), 0.0f, 1.0f);
        return SPEC_X + 1.0f + t * (SPEC_W - 2.0f);
    }

    static float specXToFreq(float px) {
        float logMin = std::log10(20.0f);
        float logMax = std::log10(20000.0f);
        float t = std::clamp((px - (SPEC_X + 1.0f)) / (SPEC_W - 2.0f), 0.0f, 1.0f);
        return std::pow(10.0f, logMin + t * (logMax - logMin));
    }

    static float eqDbToSpecY(float db) {
        float eqZeroY = SPEC_Y + SPEC_H * 0.42f;
        return std::clamp(eqZeroY - db * 3.8f, SPEC_Y + 8.0f, SPEC_Y + SPEC_H - 18.0f);
    }

    static float specYToEqDb(float py) {
        float eqZeroY = SPEC_Y + SPEC_H * 0.42f;
        return std::clamp((eqZeroY - py) / 3.8f, -8.0f, 8.0f);
    }

    int hitTestEqNode(int mx, int my) const {
        const int freqParams[4] = { PARAM_EQ_BASS_FREQ, PARAM_EQ_MUD_FREQ, PARAM_EQ_BITE_FREQ, PARAM_EQ_AIR_FREQ };
        const int gainParams[4] = { PARAM_EQ_BASS, PARAM_EQ_MUD, PARAM_EQ_BITE, PARAM_EQ_AIR };
        for (int i = 0; i < 4; ++i) {
            float nx = freqToSpecX(engine.getParameter(freqParams[i]));
            float ny = eqDbToSpecY(engine.getParameter(gainParams[i]));
            float dx = static_cast<float>(mx) - nx;
            float dy = static_cast<float>(my) - ny;
            if (dx * dx + dy * dy <= 11.0f * 11.0f) return i;
        }
        return -1;
    }

    void handleSavePresetDialog() {
        wchar_t fileBuf[MAX_PATH] = L"MyMasterPreset.json";
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = L"JSON Presets (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"json";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if (GetSaveFileNameW(&ofn)) {
            engine.savePresetJson(fileBuf);
        }
    }

    void handleLoadPresetDialog() {
        wchar_t fileBuf[MAX_PATH] = L"";
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = L"JSON Presets (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            if (engine.loadPresetJson(fileBuf)) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
    }

    void handleLoadReferenceWavDialog() {
        wchar_t fileBuf[MAX_PATH] = L"";
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFilter = L"WAV Audio (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameW(&ofn)) {
            if (refAnalyzer.loadWav(fileBuf)) {
                engine.setReferenceProfile(refAnalyzer.getProfile());
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
    }

    void onRightClick(int mx, int my) {
        int eqNode = hitTestEqNode(mx, my);
        if (eqNode >= 0) {
            const int routeParams[4] = { PARAM_EQ_BASS_ROUTE, PARAM_EQ_MUD_ROUTE, PARAM_EQ_BITE_ROUTE, PARAM_EQ_AIR_ROUTE };
            engine.pushUndoState();
            int nextRoute = (static_cast<int>(std::round(engine.getParameter(routeParams[eqNode]))) + 1) % 3;
            engine.setParameter(routeParams[eqNode], static_cast<float>(nextRoute));
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        // Right-click REF button clears reference overlay
        if (mx >= 288 && mx <= 326 && my >= 9 && my <= 33) {
            refAnalyzer.clear();
            engine.clearReferenceProfile();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }

    void onMouseDown(int mx, int my, bool isDoubleClick) {
        // 1. Draggable EQ Nodes
        int eqNode = hitTestEqNode(mx, my);
        if (eqNode >= 0) {
            const int gainParams[4] = { PARAM_EQ_BASS, PARAM_EQ_MUD, PARAM_EQ_BITE, PARAM_EQ_AIR };
            engine.pushUndoState();
            if (isDoubleClick) {
                engine.setParameter(gainParams[eqNode], 0.0f);
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                activeEqNode = eqNode;
            }
            return;
        }

        // 2. Preset Tabs
        for (const auto& tab : presetTabs) {
            if (mx >= tab.x && mx <= tab.x + tab.w && my >= tab.y && my <= tab.y + tab.h) {
                eqCurveDirty = true;
                engine.pushUndoState();
                engine.loadPreset(tab.presetId);
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }

        // 3. Calibrate (312..416, 400..422), Auto-Master (424..528, 400..422), Target LUFS drag box (424..528, 452..472)
        if (mx >= 312 && mx <= 416 && my >= 400 && my <= 422) {
            engine.triggerCalibration();
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (mx >= 424 && mx <= 528 && my >= 400 && my <= 422) {
            engine.pushUndoState();
            engine.triggerAutoMaster();
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }
        if (mx >= 424 && mx <= 528 && my >= 452 && my <= 472) {
            engine.pushUndoState();
            if (isDoubleClick) {
                engine.setParameter(PARAM_TARGET_LUFS, -14.0f);
                InvalidateRect(hwnd, nullptr, FALSE);
            } else {
                activeKnob = PARAM_TARGET_LUFS;
                dragStartY = my;
                dragStartVal = engine.getParameter(PARAM_TARGET_LUFS);
            }
            return;
        }

        // 4. Buttons
        for (const auto& btn : buttons) {
            if (mx >= btn.x && mx <= btn.x + btn.w && my >= btn.y && my <= btn.y + btn.h) {
                if (btn.customAction == 1) { engine.selectSlot(0); eqCurveDirty = true; }
                else if (btn.customAction == 2) { engine.selectSlot(1); eqCurveDirty = true; }
                else if (btn.customAction == 3) engine.copySlotAToB();
                else if (btn.customAction == 4) { engine.undo(); eqCurveDirty = true; }
                else if (btn.customAction == 5) { engine.redo(); eqCurveDirty = true; }
                else if (btn.customAction == 6) handleSavePresetDialog();
                else if (btn.customAction == 7) { handleLoadPresetDialog(); eqCurveDirty = true; }
                else if (btn.customAction == 8) handleLoadReferenceWavDialog();
                else if (btn.isToggle) {
                    eqCurveDirty = true;
                    engine.pushUndoState();
                    float cur = engine.getParameter(btn.paramId);
                    engine.setParameter(btn.paramId, (cur >= 0.5f) ? 0.0f : 1.0f);
                } else {
                    eqCurveDirty = true;
                    engine.pushUndoState();
                    engine.setParameter(btn.paramId, btn.targetValue);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return;
            }
        }

        // 5. Knobs
        for (const auto& k : knobs) {
            float dx = mx - k.x;
            float dy = my - k.y;
            float hitRadius = k.radius + 8.0f;
            if (dx * dx + dy * dy <= hitRadius * hitRadius) {
                const auto* defs = getParameterDefs();
                engine.pushUndoState();
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
        if (activeEqNode >= 0) {
            eqCurveDirty = true;
            const int freqParams[4] = { PARAM_EQ_BASS_FREQ, PARAM_EQ_MUD_FREQ, PARAM_EQ_BITE_FREQ, PARAM_EQ_AIR_FREQ };
            const int gainParams[4] = { PARAM_EQ_BASS, PARAM_EQ_MUD, PARAM_EQ_BITE, PARAM_EQ_AIR };
            engine.setParameter(freqParams[activeEqNode], specXToFreq(static_cast<float>(mx)));
            engine.setParameter(gainParams[activeEqNode], specYToEqDb(static_cast<float>(my)));
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }

            if (activeKnob >= 0) {
            eqCurveDirty = true;
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

        bool overClickable = false;
        hoverBtnIdx = -1;
        const wchar_t* newHelp = L"Drag knobs vertically (Shift = fine, Dbl-Click = reset). Lower INTENSITY (0-35%) for transparent, uncolored mastering.";

        if (hitTestEqNode(mx, my) >= 0) {
            overClickable = true;
            newHelp = L"EQ Node: Drag X=Freq, Y=Gain, Wheel=Q, Dbl-Click=Reset, Right-Click=Routing (Cyan=Stereo, Amber=Mid, Green=Side).";
        }
        if (!overClickable) {
            for (const auto& tab : presetTabs) {
                if (mx >= tab.x && mx <= tab.x + tab.w && my >= tab.y && my <= tab.y + tab.h) {
                    overClickable = true;
                    newHelp = L"Genre Preset: Loads tailored EQ, Dynamics, Saturation, and Loudness settings (Transparent uses 25% Intensity).";
                    break;
                }
            }
        }
        if (!overClickable && mx >= 312 && mx <= 416 && my >= 400 && my <= 422) {
            overClickable = true;
            newHelp = L"Calibrate Level: Listens for 1.5s and sets Input Trim to hit the -18 LUFS sweet spot without changing tone.";
        }
        if (!overClickable && mx >= 424 && mx <= 528 && my >= 400 && my <= 422) {
            overClickable = true;
            newHelp = L"Auto-Master: Analyzes 3s of your mix and dials in EQ, Dynamic EQ, Multiband, and Push for your target.";
        }
        if (!overClickable && mx >= 424 && mx <= 528 && my >= 452 && my <= 472) {
            overClickable = true;
            newHelp = L"Target LUFS Value (-24 to -3 LUFS): Drag vertically or scroll to set any loudness target (Dbl-Click = -14.0).";
        }
        if (!overClickable) {
            for (size_t i = 0; i < buttons.size(); ++i) {
                const auto& btn = buttons[i];
                if (mx >= btn.x && mx <= btn.x + btn.w && my >= btn.y && my <= btn.y + btn.h) {
                    overClickable = true;
                    hoverBtnIdx = static_cast<int>(i);
                    newHelp = btn.helpText;
                    break;
                }
            }
        }
        if (!overClickable) {
            for (const auto& k : knobs) {
                float dx = mx - k.x;
                float dy = my - k.y;
                float hitRadius = k.radius + 8.0f;
                if (dx * dx + dy * dy <= hitRadius * hitRadius) {
                    overClickable = true;
                    newHelp = k.helpText;
                    break;
                }
            }
        }
        hoverHelp = newHelp;
        isDefaultHelp = !overClickable;
        SetCursor(LoadCursor(nullptr, overClickable ? IDC_HAND : IDC_ARROW));
    }

    void onMouseWheel(int mx, int my, int delta) {
        int eqNode = hitTestEqNode(mx, my);
        if (eqNode == 1 || eqNode == 2) {
            int qParam = (eqNode == 1) ? PARAM_EQ_MUD_Q : PARAM_EQ_BITE_Q;
            const auto* defs = getParameterDefs();
            float step = (delta > 0 ? 1.0f : -1.0f) * defs[qParam].step;
            engine.setParameter(qParam, std::clamp(engine.getParameter(qParam) + step, defs[qParam].minVal, defs[qParam].maxVal));
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }

        if (mx >= 424 && mx <= 528 && my >= 452 && my <= 472) {
            const auto* defs = getParameterDefs();
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            float step = (delta > 0 ? 1.0f : -1.0f) * (shift ? 0.1f : 0.5f);
            engine.setParameter(PARAM_TARGET_LUFS, std::clamp(engine.getParameter(PARAM_TARGET_LUFS) + step, defs[PARAM_TARGET_LUFS].minVal, defs[PARAM_TARGET_LUFS].maxVal));
            InvalidateRect(hwnd, nullptr, FALSE);
            return;
        }

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

            D2D1_STROKE_STYLE_PROPERTIES dp = D2D1::StrokeStyleProperties(
                D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
                D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_DASH, 0.0f
            );
            float dashes[] = { 3.0f, 3.0f };
            pD2DFactory->CreateStrokeStyle(&dp, dashes, 2, &pDashedStrokeStyle);
        }

        AssterUI::UITheme::initTextFormats(
            pDWriteFactory, &pFormatTitle, &pFormatSubtitle, &pFormatModule,
            &pFormatKnobTitle, &pFormatKnobVal, &pFormatButton, &pFormatButtonSub,
            &pFormatSmall, &pFormatMeterVal, &pFormatMeterTag
        );
    }

    void cleanupDirect2D() {
        discardDeviceResources();
        if (pDashedStrokeStyle){ pDashedStrokeStyle->Release(); pDashedStrokeStyle = nullptr; }
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

        auto addKnob = [&](int id, float x, float y, float r, const char* title, const wchar_t* help, bool hero = false) {
            std::string s(title);
            std::wstring ws(s.begin(), s.end());
            knobs.push_back({ id, x, y, r, s, ws, help, hero });
        };

        auto addButton = [&](int id, float x, float y, float w, float h, const char* text, float targetVal, bool isToggle, const wchar_t* help, int customAction = 0) {
            std::string s(text);
            std::wstring ws(s.begin(), s.end());
            buttons.push_back({ id, customAction, x, y, w, h, s, ws, help, targetVal, isToggle });
        };

        // Top Control Bar (y = 9..33)
        addButton(-1, 16,  9, 24, 24, "A",    0.0f, false, L"Slot A: Switch to comparison state A.", 1);
        addButton(-1, 42,  9, 24, 24, "B",    0.0f, false, L"Slot B: Switch to comparison state B.", 2);
        addButton(-1, 68,  9, 34, 24, "A>B",  0.0f, false, L"Copy A to B: Duplicate Slot A settings into Slot B.", 3);

        addButton(-1, 110, 9, 42, 24, "UNDO", 0.0f, false, L"Undo (Ctrl+Z): Revert the last parameter or preset change.", 4);
        addButton(-1, 154, 9, 42, 24, "REDO", 0.0f, false, L"Redo (Ctrl+Y): Re-apply an undone change.", 5);

        addButton(-1, 204, 9, 40, 24, "SAVE", 0.0f, false, L"Save Preset: Export your current settings to a .json preset file.", 6);
        addButton(-1, 246, 9, 40, 24, "LOAD", 0.0f, false, L"Load Preset: Import a saved .json preset file.", 7);
        addButton(-1, 288, 9, 38, 24, "REF",  0.0f, false, L"Reference WAV: Load a .wav track to overlay its spectral curve (Right-Click to clear).", 8);

        addButton(PARAM_OVERSAMPLING, 356, 9, 26, 24, "1X", 0.0f, false, L"1x Oversampling: Lowest CPU usage, standard sample rate.");
        addButton(PARAM_OVERSAMPLING, 384, 9, 26, 24, "2X", 1.0f, false, L"2x Oversampling: Oversamples saturation, clipping, AND brickwall limiting.");
        addButton(PARAM_OVERSAMPLING, 412, 9, 26, 24, "4X", 2.0f, false, L"4x Oversampling: Highest precision anti-aliased limiting & saturation.");

        addButton(PARAM_LIMITER_STYLE, 476, 9, 46, 24, "CLEAN", 0.0f, false, L"Limiter CLEAN: Maximum transparency for uncolored, streaming-ready masters.");
        addButton(PARAM_LIMITER_STYLE, 524, 9, 46, 24, "PUNCH", 1.0f, false, L"Limiter PUNCH: Preserves drum transients and snap under loud limiting.");
        addButton(PARAM_LIMITER_STYLE, 572, 9, 46, 24, "CRUSH", 2.0f, false, L"Limiter CRUSH: Aggressive analog-style loudness for extreme metal & djent.");

        addButton(PARAM_DITHER_MODE, 668, 9, 30, 24, "OFF", 0.0f, false, L"Dither OFF: 32-bit floating-point output (use if DAW handles dithering).");
        addButton(PARAM_DITHER_MODE, 700, 9, 30, 24, "24B", 1.0f, false, L"24-Bit TPDF Dither: Noise-shaped dithering for 24-bit export.");
        addButton(PARAM_DITHER_MODE, 732, 9, 30, 24, "16B", 2.0f, false, L"16-Bit TPDF Dither: Noise-shaped dithering for 16-bit CD/WAV export.");

        addButton(PARAM_MATCH_BYPASS, 922,  9, 74, 24, "MATCH",     1.0f, true, L"Loudness Match: Matches bypassed volume to mastered volume for honest A/B.");
        addButton(PARAM_BYPASS,       1004, 9, 100, 24, "MASTER ON", 1.0f, true, L"Master Bypass: Toggle all mastering processing on or off.");

        // Module Power Toggles on Card Headers
        addButton(PARAM_MOD_EQ,     214, 247, 32, 16, "EQ",   1.0f, true, L"EQ Power: Bypass or enable the 4-band Mastering EQ.");
        addButton(PARAM_MOD_DYN,    248, 247, 34, 16, "DYN",  1.0f, true, L"Dynamic EQ Power: Bypass or enable Chug & De-Harsh dynamic EQ.");
        addButton(PARAM_MOD_MB,     758, 247, 32, 16, "MB",   1.0f, true, L"Multiband Power: Bypass or enable the 3-band LR4 Multiband Compressor.");
        addButton(PARAM_MOD_GLUE,   794, 247, 40, 16, "GLUE", 1.0f, true, L"Bus Glue Power: Bypass or enable the SSL-style Bus Glue Compressor.");
        addButton(PARAM_MOD_SAT,    1020,247, 36, 16, "SAT",  1.0f, true, L"Saturator Power: Bypass or enable analog harmonic saturation.");
        addButton(PARAM_MOD_STEREO, 1058,247, 38, 16, "WID",  1.0f, true, L"Stereo Power: Bypass or enable M/S Stereo Width processing.");

        // Target LUFS Lock Toggle in Card 2
        addButton(PARAM_TARGET_LUFS_ON, 312, 452, 104, 20, "TARGET LUFS", 1.0f, true, L"Target LUFS Lock: Automatically adjusts limiter drive to hold your chosen loudness target.");

        // 9 Presets Ribbon (y = 48..78, x = 16..1104)
        float startX = 16.0f;
        float tabW = 116.0f;
        float tabH = 30.0f;
        float spacing = 5.0f;

        presetTabs.push_back({ 0, startX + 0 * (tabW + spacing), 48.0f, tabW, tabH, "EXTREME",     "-8.0 LUFS" });
        presetTabs.push_back({ 1, startX + 1 * (tabW + spacing), 48.0f, tabW, tabH, "DOOM",        "-10.5 LUFS" });
        presetTabs.push_back({ 2, startX + 2 * (tabW + spacing), 48.0f, tabW, tabH, "SLUDGE",      "-9.0 LUFS" });
        presetTabs.push_back({ 3, startX + 3 * (tabW + spacing), 48.0f, tabW, tabH, "GROOVE",      "-8.5 LUFS" });
        presetTabs.push_back({ 4, startX + 4 * (tabW + spacing), 48.0f, tabW, tabH, "DJENT",       "-7.5 LUFS" });
        presetTabs.push_back({ 5, startX + 5 * (tabW + spacing), 48.0f, tabW, tabH, "CLASSIC",     "-11.0 LUFS" });
        presetTabs.push_back({ 6, startX + 6 * (tabW + spacing), 48.0f, tabW, tabH, "TRANSPARENT", "-14.0 LUFS" });
        presetTabs.push_back({ 7, startX + 7 * (tabW + spacing), 48.0f, tabW, tabH, "POP",         "-9.5 LUFS" });
        presetTabs.push_back({ 8, startX + 8 * (tabW + spacing), 48.0f, tabW + 4.0f, tabH, "HIP-HOP", "-8.5 LUFS" });

        // Card 1: TONAL & DYNAMIC EQ (x: 16, y: 242, w: 274, h: 328)
        addKnob(PARAM_EQ_BASS,   64,  300, 19, "BASS PUNCH",  L"Bass Punch: Low-shelf boost or cut for kick weight and sub foundation.");
        addKnob(PARAM_EQ_MUD,    153, 300, 19, "MUD SCOOP",   L"Mud Scoop: Midrange cut/boost to clear boxy low-mids around 250-600 Hz.");
        addKnob(PARAM_EQ_BITE,   242, 300, 19, "BITE ATTACK", L"Bite Attack: Upper-mid presence for guitar pick attack and snare crack.");
        addKnob(PARAM_EQ_AIR,    64,  390, 19, "AIR SHEEN",   L"Air Sheen: High-shelf sparkle and clarity above 8-12 kHz.");
        addKnob(PARAM_SUB_CUT,   153, 390, 19, "SUB CUT",     L"Sub Cut (HPF): Steep 24 dB/oct filter removing inaudible sub-bass rumble.");
        addKnob(PARAM_DYN_CHUG,  242, 390, 19, "CHUG DYN",    L"Chug Dynamic EQ: Automatically tames palm-mute low-mid spikes around 220 Hz.");
        addKnob(PARAM_DYN_HARSH, 64,  482, 19, "DE-HARSH",    L"De-Harsh Dynamic EQ: Automatically tames harsh cymbal & fizz spikes around 4.2 kHz.");
        addKnob(PARAM_STABILIZE, 144, 482, 19, "STABILIZE",   L"Spectral Stabilizer: Continuously balances 5 frequency bands in real time toward your target.");

        // Card 2: MASTER PUSH & ASSISTANT (x: 298, y: 242, w: 244, h: 328)
        addKnob(PARAM_MASTER_PUSH, 420, 304, 32, "MASTER PUSH",    L"Master Push: Drives the mix into the true-peak limiter for commercial loudness.", true);
        addButton(PARAM_AUTOMASTER_MODE, 312, 376, 68, 18, "INV",  0.0f, false, L"Auto-Master INVISIBLE: Subtle +/-1.5dB EQ, 30% Intensity, Clean Limiter + Target LUFS.");
        addButton(PARAM_AUTOMASTER_MODE, 386, 376, 68, 18, "POL",  1.0f, false, L"Auto-Master POLISH: Balanced +/-3dB EQ, 75% Intensity, Punch Limiter.");
        addButton(PARAM_AUTOMASTER_MODE, 460, 376, 68, 18, "LOUD", 2.0f, false, L"Auto-Master LOUD: Full commercial metal sculpting, 100% Intensity, Crush Limiter.");
        addKnob(PARAM_INTENSITY,   362, 508, 18, "INTENSITY",      L"Global Intensity (0-100%): Scales all EQ/Color/Dynamics. Use 0-35% to keep your unmastered sound!");
        addKnob(PARAM_CEILING,     478, 508, 18, "CEILING (dBTP)", L"True-Peak Ceiling: Absolute maximum output peak. Guarantees 0 digital clipping.");

        // Card 3: MULTIBAND & TRANSIENTS (x: 550, y: 242, w: 292, h: 328)
        addKnob(PARAM_MB_LOW,        602, 298, 18, "MB LOW",     L"Multiband Low (<160 Hz): Controls sub-bass and kick weight independently.");
        addKnob(PARAM_MB_MID,        696, 298, 18, "MB MID",     L"Multiband Mid (160-2.5k Hz): Evens out guitar & vocal density.");
        addKnob(PARAM_MB_HIGH,       790, 298, 18, "MB HIGH",    L"Multiband High (>2.5 kHz): Smooths aggressive cymbals and top-end.");
        addKnob(PARAM_TRANS_PUNCH,   602, 398, 18, "PUNCH",      L"Transient Punch: Restores or softens drum attack before the limiter.");
        addKnob(PARAM_TRANS_SUSTAIN, 696, 398, 18, "SUSTAIN",    L"Transient Sustain: Controls room tail and body length between hits.");
        addKnob(PARAM_CLIPPER_DRIVE, 790, 398, 18, "CLIP DRIVE", L"Soft Clipper Drive: Shaves fast micro-peaks transparently before limiting.");
        addKnob(PARAM_GLUE_AMOUNT,   602, 492, 18, "BUS GLUE",   L"Bus Glue: SSL-style VCA compressor that glues drums and guitars together.");
        addKnob(PARAM_GLUE_SC_HPF,   696, 492, 18, "SC HPF",     L"Glue Sidechain HPF: Keeps deep kick/sub from pumping the bus compressor.");
        addKnob(PARAM_GLUE_MIX,      790, 492, 18, "GLUE MIX",   L"Glue Parallel Mix: Blends dry transients with compressed signal.");

        // Card 4: COLOR, STEREO & TRIM (x: 850, y: 242, w: 254, h: 328)
        addKnob(PARAM_SAT_DRIVE,    896,  300, 18, "SAT DRIVE",    L"Saturation Drive: Adds warm Tape, Tube, or Console harmonic density.");
        addKnob(PARAM_SAT_MIX,      977,  300, 18, "SAT MIX",      L"Saturation Mix: Parallel dry/wet blend for harmonic saturation.");
        addKnob(PARAM_STEREO_WIDTH, 1058, 300, 18, "WIDTH",        L"Stereo Width: Expands or narrows side information (100% = neutral).");
        addKnob(PARAM_MONO_FREQ,    930,  396, 18, "MONO BASS",    L"Mono Bass Crossover: Sums bass below this frequency to tight mono.");
        addKnob(PARAM_IN_TRIM,      1024, 396, 18, "INPUT TRIM",   L"Input Trim: Manual gain adjustment before processing (-24 to +24 dB).");

        addButton(PARAM_SAT_TYPE, 868,  482, 70, 22, "TAPE",  0.0f, false, L"Tape Saturation: Smooth, warm odd/even compression curve.");
        addButton(PARAM_SAT_TYPE, 942,  482, 70, 22, "TUBE",  1.0f, false, L"Tube Saturation: Asymmetric triode warmth and 2nd-order harmonics.");
        addButton(PARAM_SAT_TYPE, 1016, 482, 70, 22, "SOLID", 2.0f, false, L"Console Saturation: Punchy solid-state analog clipping.");

        addButton(PARAM_AUTOTRIM_MODE, 868,  526, 70, 22, "OFF",   0.0f, false, L"AutoTrim OFF: Uses only your manual Input Trim setting.");
        addButton(PARAM_AUTOTRIM_MODE, 942,  526, 70, 22, "LEARN", 1.0f, false, L"AutoTrim LEARN: Locks in gain measured by the Calibrate button.");
        addButton(PARAM_AUTOTRIM_MODE, 1016, 526, 70, 22, "CONT",  2.0f, false, L"AutoTrim CONTINUOUS: Smoothly levels incoming loudness in real time.");
    }

    void render() {
        std::lock_guard<std::mutex> lock(renderMutex);
        if (!pRenderTarget) {
            createDeviceResources();
            if (!pRenderTarget) return;
        }

        pRenderTarget->BeginDraw();

        setBrush(AssterUI::Colors::BgChassis);
        pRenderTarget->FillRectangle(D2D1::RectF(0, 0, UI_WIDTH, UI_HEIGHT), pSolidBrush);
        setBrush(AssterUI::Colors::BorderChassis);
        pRenderTarget->DrawRectangle(D2D1::RectF(0.5f, 0.5f, UI_WIDTH - 0.5f, UI_HEIGHT - 0.5f), pSolidBrush, 1.0f);

        drawHeader();
        drawPresetRibbon();
        drawAnalyzerStrip();

        drawModuleCard(16,  242, 274, 328, L"TONAL & DYNAMIC EQ");
        drawModuleCard(298, 242, 244, 328, L"MASTER PUSH & ASSISTANT");
        drawModuleCard(550, 242, 292, 328, L"MULTIBAND & TRANSIENTS");
        drawModuleCard(850, 242, 254, 328, L"COLOR, STEREO & TRIM");

        drawDynamicEQMeters();
        drawMultibandGRBars();

        setBrush(AssterUI::Colors::TextMuted);
        drawText(L"SATURATOR TYPE", 868,  464, 218, 16, pFormatKnobTitle);
        drawText(L"AUTOTRIM MODE",  868,  508, 218, 16, pFormatKnobTitle);

        for (size_t i = 0; i < buttons.size(); ++i) {
            drawButton(buttons[i], static_cast<int>(i) == hoverBtnIdx);
        }
        for (const auto& k : knobs) {
            drawKnob(k);
        }

        drawAssistantModule();
        drawMeteringDeck();
        drawHelpBar();

        HRESULT hr = pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            discardDeviceResources();
        }
    }

    void drawHeader() {
        setBrush(AssterUI::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(0, 0, UI_WIDTH, 42), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawLine(D2D1::Point2F(0, 42), D2D1::Point2F(UI_WIDTH, 42), pSolidBrush, 1.0f);

        setBrush(AssterUI::Colors::TextMuted);
        drawText(L"OS",   332, 9, 22, 24, pFormatKnobTitle);
        drawText(L"LIM",  446, 9, 28, 24, pFormatKnobTitle);
        drawText(L"DITH", 626, 9, 40, 24, pFormatKnobTitle);
    }

    void drawPresetRibbon() {
        int curPreset = engine.getCurrentPreset();
        for (const auto& tab : presetTabs) {
            bool active = (tab.presetId == curPreset);
            D2D1_RECT_F r = D2D1::RectF(tab.x, tab.y, tab.x + tab.w, tab.y + tab.h);
            std::wstring wName(tab.name, tab.name + strlen(tab.name));
            std::wstring wTag(tab.targetLufs, tab.targetLufs + strlen(tab.targetLufs));

            if (active) {
                setBrush(AssterUI::Colors::ButtonActiveBg);
                pRenderTarget->FillRectangle(r, pSolidBrush);
                setBrush(AssterUI::Colors::ButtonActiveText);
                drawText(wName.c_str(), tab.x, tab.y + 1.0f, tab.w, 15.0f, pFormatButton);
                drawText(wTag.c_str(),  tab.x, tab.y + 15.0f, tab.w, 12.0f, pFormatButtonSub);
            } else {
                setBrush(AssterUI::Colors::ButtonInactiveBg);
                pRenderTarget->FillRectangle(r, pSolidBrush);
                setBrush(AssterUI::Colors::ButtonInactiveBorder);
                pRenderTarget->DrawRectangle(r, pSolidBrush, 1.0f);
                setBrush(AssterUI::Colors::TextActive);
                drawText(wName.c_str(), tab.x, tab.y + 1.0f, tab.w, 15.0f, pFormatButton);
                setBrush(AssterUI::Colors::TextDim);
                drawText(wTag.c_str(),  tab.x, tab.y + 15.0f, tab.w, 12.0f, pFormatButtonSub);
            }
        }
    }

    void drawAnalyzerStrip() {
        drawSpectrumAndEQ(SPEC_X, SPEC_Y, SPEC_W, SPEC_H);
        drawGRHistory(714.0f, 84.0f, 262.0f, 150.0f);
        drawVectorscope(984.0f, 84.0f, 120.0f, 150.0f);
    }

    void drawSpectrumAndEQ(float x, float y, float w, float h) {
        setBrush(AssterUI::Colors::BgInset);
        pRenderTarget->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(x + 0.5f, y + 0.5f, x + w - 0.5f, y + h - 0.5f), pSolidBrush, 1.0f);

        const float freqs[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
        const wchar_t* labels[] = { L"50", L"100", L"250", L"500", L"1k", L"2k", L"5k", L"10k" };
        float logMin = std::log10(20.0f);
        float logMax = std::log10(20000.0f);

        for (int i = 0; i < 8; ++i) {
            float tx = x + ((std::log10(freqs[i]) - logMin) / (logMax - logMin)) * w;
            setBrush(AssterUI::Colors::GridLine);
            pRenderTarget->DrawLine(D2D1::Point2F(tx, y + 1.0f), D2D1::Point2F(tx, y + h - 14.0f), pSolidBrush, 1.0f);
            setBrush(AssterUI::Colors::TextDim);
            drawText(labels[i], tx - 14.0f, y + h - 14.0f, 28.0f, 13.0f, pFormatButtonSub);
        }

        float eqZeroY = y + h * 0.42f;
        setBrush(AssterUI::Colors::GridLine);
        pRenderTarget->DrawLine(D2D1::Point2F(x + 1.0f, eqZeroY), D2D1::Point2F(x + w - 1.0f, eqZeroY), pSolidBrush, 1.0f);

        constexpr int NUM_BINS = 96;
        engine.copyPreFFTSamples(preFftSamples, LoudnessMeter::FFT_SIZE);
        preFftProcessor.compute(preFftSamples, 44100.0f, preFftBinsDb, NUM_BINS, 20.0f, 20000.0f);

        engine.copyFFTSamples(fftSamples, LoudnessMeter::FFT_SIZE);
        postFftProcessor.compute(fftSamples, 44100.0f, fftBinsDb, NUM_BINS, 20.0f, 20000.0f);

        auto drawFftPoly = [&](const float* bins, const AssterUI::D2DColor& fillCol) {
            if (!pD2DFactory) return;
            ID2D1PathGeometry* pSpecPath = nullptr;
            if (SUCCEEDED(pD2DFactory->CreatePathGeometry(&pSpecPath))) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pSpecPath->Open(&pSink))) {
                    float plotH = h - 14.0f;
                    pSink->BeginFigure(D2D1::Point2F(x + 1.0f, y + plotH), D2D1_FIGURE_BEGIN_FILLED);
                    for (int i = 0; i < NUM_BINS; ++i) {
                        float px = x + 1.0f + (static_cast<float>(i) / static_cast<float>(NUM_BINS - 1)) * (w - 2.0f);
                        float norm = std::clamp((bins[i] + 90.0f) / 90.0f, 0.0f, 1.0f);
                        float py = y + plotH - norm * (plotH - 8.0f);
                        pSink->AddLine(D2D1::Point2F(px, py));
                    }
                    pSink->AddLine(D2D1::Point2F(x + w - 1.0f, y + plotH));
                    pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    pSink->Close();
                    pSink->Release();
                    setBrush(fillCol);
                    pRenderTarget->FillGeometry(pSpecPath, pSolidBrush);
                }
                pSpecPath->Release();
            }
        };

        drawFftPoly(preFftBinsDb, AssterUI::Colors::PreSpectrumFill);
        drawFftPoly(fftBinsDb, AssterUI::Colors::SpectrumFill);

        // Reference Track Spectral Overlay (if loaded)
        if (refAnalyzer.isLoaded() && pD2DFactory) {
            const float* refBins = refAnalyzer.getBinsDb();
            ID2D1PathGeometry* pRefPath = nullptr;
            if (SUCCEEDED(pD2DFactory->CreatePathGeometry(&pRefPath))) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pRefPath->Open(&pSink))) {
                    float plotH = h - 14.0f;
                    for (int i = 0; i < NUM_BINS; ++i) {
                        float px = x + 1.0f + (static_cast<float>(i) / static_cast<float>(NUM_BINS - 1)) * (w - 2.0f);
                        float norm = std::clamp((refBins[i] + 90.0f) / 90.0f, 0.0f, 1.0f);
                        float py = y + plotH - norm * (plotH - 8.0f);
                        if (i == 0) pSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                        else pSink->AddLine(D2D1::Point2F(px, py));
                    }
                    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
                    pSink->Close();
                    pSink->Release();
                    setBrush(AssterUI::Colors::AccentAmber);
                    pRenderTarget->DrawGeometry(pRefPath, pSolidBrush, 1.4f, pDashedStrokeStyle);
                }
                pRefPath->Release();
            }
        }

        // EQ Transfer Curve (cached, recomputed only when EQ parameters change)
        if (pD2DFactory) {
            constexpr int EQ_PTS = 96;
            if (eqCurveDirty) {
                for (int i = 0; i < EQ_PTS; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(EQ_PTS - 1);
                    float freq = std::pow(10.0f, logMin + t * (logMax - logMin));
                    float magDb = std::clamp(engine.getEQMagnitudeDb(freq), -14.0f, 12.0f);
                    cachedEqY[i] = eqDbToSpecY(magDb);
                }
                eqCurveDirty = false;
            }
            ID2D1PathGeometry* pEqPath = nullptr;
            if (SUCCEEDED(pD2DFactory->CreatePathGeometry(&pEqPath))) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pEqPath->Open(&pSink))) {
                    for (int i = 0; i < EQ_PTS; ++i) {
                        float t = static_cast<float>(i) / static_cast<float>(EQ_PTS - 1);
                        float px = x + 1.0f + t * (w - 2.0f);
                        float py = cachedEqY[i];
                        if (i == 0) pSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                        else pSink->AddLine(D2D1::Point2F(px, py));
                    }
                    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
                    pSink->Close();
                    pSink->Release();
                    setBrush(AssterUI::Colors::AccentCyan);
                    pRenderTarget->DrawGeometry(pEqPath, pSolidBrush, 1.8f, pRoundStrokeStyle);
                }
                pEqPath->Release();
            }
        }

        // 4 Interactive Draggable EQ Nodes (Color-coded by routing: Cyan=ST, Amber=MID, Green=SIDE)
        const int freqParams[4]  = { PARAM_EQ_BASS_FREQ, PARAM_EQ_MUD_FREQ, PARAM_EQ_BITE_FREQ, PARAM_EQ_AIR_FREQ };
        const int gainParams[4]  = { PARAM_EQ_BASS, PARAM_EQ_MUD, PARAM_EQ_BITE, PARAM_EQ_AIR };
        const int routeParams[4] = { PARAM_EQ_BASS_ROUTE, PARAM_EQ_MUD_ROUTE, PARAM_EQ_BITE_ROUTE, PARAM_EQ_AIR_ROUTE };
        const wchar_t* nodeTags[4] = { L"1", L"2", L"3", L"4" };

        for (int i = 0; i < 4; ++i) {
            float nx = freqToSpecX(engine.getParameter(freqParams[i]));
            float ny = eqDbToSpecY(engine.getParameter(gainParams[i]));
            int rt = static_cast<int>(std::round(engine.getParameter(routeParams[i])));
            AssterUI::D2DColor ringCol = (rt == 1) ? AssterUI::Colors::AccentAmber
                                       : (rt == 2 ? AssterUI::Colors::AccentGreen : AssterUI::Colors::AccentCyan);
            bool isSel = (activeEqNode == i);

            setBrush(isSel ? AssterUI::Colors::TextBright : AssterUI::Colors::BgHeader);
            pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(nx, ny), 7.5f, 7.5f), pSolidBrush);
            setBrush(ringCol);
            pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(nx, ny), 7.5f, 7.5f), pSolidBrush, isSel ? 2.0f : 1.5f);

            setBrush(isSel ? AssterUI::Colors::ButtonActiveText : AssterUI::Colors::TextBright);
            drawText(nodeTags[i], nx - 7.0f, ny - 7.0f, 14.0f, 14.0f, pFormatButtonSub);
        }

        setBrush(AssterUI::Colors::TextMuted);
        if (refAnalyzer.isLoaded()) {
            std::wstring lbl = L"PRE/POST SPECTRUM + REF (" + refAnalyzer.getFileName() + L")";
            drawText(lbl.c_str(), x + 8.0f, y + 4.0f, 440.0f, 14.0f, pFormatSmall);
        } else {
            drawText(L"PRE/POST SPECTRUM & INTERACTIVE EQ (R-CLICK NODE = ST/MID/SIDE)", x + 8.0f, y + 4.0f, 440.0f, 14.0f, pFormatSmall);
        }
    }

    void drawGRHistory(float x, float y, float w, float h) {
        setBrush(AssterUI::Colors::BgInset);
        pRenderTarget->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(x + 0.5f, y + 0.5f, x + w - 0.5f, y + h - 0.5f), pSolidBrush, 1.0f);

        engine.copyHistory(historyPts, LoudnessMeter::HISTORY_SIZE);

        float midY = y + h * 0.5f;
        setBrush(AssterUI::Colors::GridLine);
        pRenderTarget->DrawLine(D2D1::Point2F(x + 1.0f, midY), D2D1::Point2F(x + w - 1.0f, midY), pSolidBrush, 1.0f);

        if (pD2DFactory) {
            constexpr int N = LoudnessMeter::HISTORY_SIZE;
            setBrush(AssterUI::Colors::SpectrumFill);
            for (int i = 0; i < N; i += 2) {
                float px = x + 1.0f + (static_cast<float>(i) / static_cast<float>(N - 1)) * (w - 2.0f);
                float pk = std::clamp(historyPts[i].peak, 0.0f, 1.0f);
                float bh = pk * (h - 22.0f);
                if (bh > 1.0f) {
                    pRenderTarget->FillRectangle(D2D1::RectF(px, y + h - 2.0f - bh, px + 1.5f, y + h - 2.0f), pSolidBrush);
                }
            }

            ID2D1PathGeometry* pLimPath = nullptr;
            if (SUCCEEDED(pD2DFactory->CreatePathGeometry(&pLimPath))) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pLimPath->Open(&pSink))) {
                    for (int i = 0; i < N; ++i) {
                        float px = x + 1.0f + (static_cast<float>(i) / static_cast<float>(N - 1)) * (w - 2.0f);
                        float grNorm = std::clamp(historyPts[i].limGr / 12.0f, 0.0f, 1.0f);
                        float py = y + 18.0f + grNorm * (h - 24.0f);
                        if (i == 0) pSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                        else pSink->AddLine(D2D1::Point2F(px, py));
                    }
                    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
                    pSink->Close();
                    pSink->Release();
                    setBrush(AssterUI::Colors::AccentRed);
                    pRenderTarget->DrawGeometry(pLimPath, pSolidBrush, 1.6f, pRoundStrokeStyle);
                }
                pLimPath->Release();
            }

            ID2D1PathGeometry* pGluePath = nullptr;
            if (SUCCEEDED(pD2DFactory->CreatePathGeometry(&pGluePath))) {
                ID2D1GeometrySink* pSink = nullptr;
                if (SUCCEEDED(pGluePath->Open(&pSink))) {
                    for (int i = 0; i < N; ++i) {
                        float px = x + 1.0f + (static_cast<float>(i) / static_cast<float>(N - 1)) * (w - 2.0f);
                        float grNorm = std::clamp(historyPts[i].glueGr / 12.0f, 0.0f, 1.0f);
                        float py = y + 18.0f + grNorm * (h - 24.0f);
                        if (i == 0) pSink->BeginFigure(D2D1::Point2F(px, py), D2D1_FIGURE_BEGIN_HOLLOW);
                        else pSink->AddLine(D2D1::Point2F(px, py));
                    }
                    pSink->EndFigure(D2D1_FIGURE_END_OPEN);
                    pSink->Close();
                    pSink->Release();
                    setBrush(AssterUI::Colors::AccentAmber);
                    pRenderTarget->DrawGeometry(pGluePath, pSolidBrush, 1.2f, pRoundStrokeStyle);
                }
                pGluePath->Release();
            }
        }

        setBrush(AssterUI::Colors::TextMuted);
        drawText(L"PEAK & GR HISTORY", x + 8.0f, y + 4.0f, 160.0f, 14.0f, pFormatSmall);
    }

    void drawVectorscope(float x, float y, float w, float h) {
        setBrush(AssterUI::Colors::BgInset);
        pRenderTarget->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(x + 0.5f, y + 0.5f, x + w - 0.5f, y + h - 0.5f), pSolidBrush, 1.0f);

        float cx = x + w * 0.5f;
        float cy = y + (h - 18.0f) * 0.5f + 6.0f;
        float rad = 46.0f;

        setBrush(AssterUI::Colors::GridLine);
        pRenderTarget->DrawLine(D2D1::Point2F(cx - rad, cy), D2D1::Point2F(cx + rad, cy), pSolidBrush, 1.0f);
        pRenderTarget->DrawLine(D2D1::Point2F(cx, cy - rad), D2D1::Point2F(cx, cy + rad), pSolidBrush, 1.0f);

        engine.copyScopeSamples(scopeL, scopeR, LoudnessMeter::SCOPE_SIZE);
        setBrush(AssterUI::Colors::AccentCyan);
        for (int i = 0; i < LoudnessMeter::SCOPE_SIZE; i += 2) {
            float l = scopeL[i];
            float r = scopeR[i];
            float sx = (l - r) * 0.7071f;
            float sy = (l + r) * 0.7071f;
            float px = std::clamp(cx + sx * rad, x + 4.0f, x + w - 4.0f);
            float py = std::clamp(cy - sy * rad, y + 16.0f, y + h - 20.0f);
            pRenderTarget->FillRectangle(D2D1::RectF(px, py, px + 1.5f, py + 1.5f), pSolidBrush);
        }

        float corr = engine.getCorrelation();
        float barX = x + 8.0f;
        float barY = y + h - 12.0f;
        float barW = w - 16.0f;
        float barH = 6.0f;
        setBrush(AssterUI::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(barX, barY, barX + barW, barY + barH), pSolidBrush);

        float normPos = std::clamp((corr + 1.0f) * 0.5f, 0.0f, 1.0f);
        float midX = barX + barW * 0.5f;
        float indX = barX + normPos * barW;
        setBrush(corr >= 0.0f ? AssterUI::Colors::AccentGreen : AssterUI::Colors::AccentRed);
        pRenderTarget->FillRectangle(D2D1::RectF(std::min(midX, indX), barY, std::max(midX, indX), barY + barH), pSolidBrush);

        setBrush(AssterUI::Colors::TextMuted);
        drawText(L"VECTOR", x + 8.0f, y + 4.0f, 80.0f, 14.0f, pFormatSmall);
    }

    void drawDynamicEQMeters() {
        float chugGr = engine.getChugGR();
        float harshGr = engine.getHarshGR();

        auto drawMiniBar = [&](float bx, float by, float bw, const wchar_t* label, float valDb) {
            setBrush(AssterUI::Colors::TextMuted);
            drawText(label, bx, by, bw, 12.0f, pFormatSmall);

            float trackY = by + 13.0f;
            setBrush(AssterUI::Colors::BgHeader);
            pRenderTarget->FillRectangle(D2D1::RectF(bx, trackY, bx + bw, trackY + 6.0f), pSolidBrush);
            setBrush(AssterUI::Colors::BorderCard);
            pRenderTarget->DrawRectangle(D2D1::RectF(bx + 0.5f, trackY + 0.5f, bx + bw - 0.5f, trackY + 5.5f), pSolidBrush, 1.0f);

            float fillW = std::clamp(valDb / 6.0f, 0.0f, 1.0f) * bw;
            if (fillW > 1.0f) {
                setBrush(AssterUI::Colors::AccentAmber);
                pRenderTarget->FillRectangle(D2D1::RectF(bx, trackY, bx + fillW, trackY + 6.0f), pSolidBrush);
            }
        };

        drawMiniBar(194.0f, 464.0f, 86.0f, L"CHUG ATTEN", chugGr);
        drawMiniBar(194.0f, 492.0f, 86.0f, L"HARSH ATTEN", harshGr);
    }

    void drawMultibandGRBars() {
        auto drawBandBar = [&](float cx, float y, float grDb) {
            float bw = 64.0f;
            float bx = cx - bw * 0.5f;
            setBrush(AssterUI::Colors::BgHeader);
            pRenderTarget->FillRectangle(D2D1::RectF(bx, y, bx + bw, y + 5.0f), pSolidBrush);
            float fillW = std::clamp(grDb / 8.0f, 0.0f, 1.0f) * bw;
            if (fillW > 1.0f) {
                setBrush(AssterUI::Colors::AccentAmber);
                pRenderTarget->FillRectangle(D2D1::RectF(bx, y, bx + fillW, y + 5.0f), pSolidBrush);
            }
        };
        drawBandBar(602.0f, 353.0f, engine.getMBLowGR());
        drawBandBar(696.0f, 353.0f, engine.getMBMidGR());
        drawBandBar(790.0f, 353.0f, engine.getMBHighGR());
    }

    void drawModuleCard(float x, float y, float w, float h, const wchar_t* title) {
        setBrush(AssterUI::Colors::BgCard);
        pRenderTarget->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(x + 0.5f, y + 0.5f, x + w - 0.5f, y + h - 0.5f), pSolidBrush, 1.0f);
        setBrush(AssterUI::Colors::TextActive);
        drawText(title, x + 12, y + 7, w - 24, 18, pFormatModule);
    }

    void drawButton(const AssterUI::ButtonWidget& btn, bool hovered) {
        bool active = false;
        bool disabled = false;
        if (btn.customAction == 1) active = (engine.getActiveSlot() == 0);
        else if (btn.customAction == 2) active = (engine.getActiveSlot() == 1);
        else if (btn.customAction == 4) disabled = !engine.canUndo();
        else if (btn.customAction == 5) disabled = !engine.canRedo();
        else if (btn.customAction == 8) active = refAnalyzer.isLoaded();
        else if (btn.customAction == 0) {
            float cur = engine.getParameter(btn.paramId);
            active = btn.isToggle ? (cur >= 0.5f) : (std::abs(cur - btn.targetValue) < 0.25f);
        }

        D2D1_RECT_F r = D2D1::RectF(btn.x, btn.y, btn.x + btn.w, btn.y + btn.h);

        if (btn.paramId == PARAM_BYPASS) {
            if (active) {
                setBrush(AssterUI::Colors::AccentRed);
                pRenderTarget->FillRectangle(r, pSolidBrush);
                setBrush(AssterUI::Colors::ButtonActiveText);
                drawText(L"BYPASSED", btn.x, btn.y, btn.w, btn.h, pFormatButton);
            } else {
                setBrush(hovered ? AssterUI::Colors::ButtonHoverBg : AssterUI::Colors::ButtonInactiveBg);
                pRenderTarget->FillRectangle(r, pSolidBrush);
                setBrush(AssterUI::Colors::ButtonInactiveBorder);
                pRenderTarget->DrawRectangle(r, pSolidBrush, 1.0f);
                setBrush(AssterUI::Colors::AccentGreen);
                pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(btn.x + 12.0f, btn.y + btn.h * 0.5f), 3.5f, 3.5f), pSolidBrush);
                setBrush(AssterUI::Colors::TextActive);
                drawText(L"MASTER ON", btn.x + 8.0f, btn.y, btn.w - 8.0f, btn.h, pFormatButton);
            }
            return;
        }

        if (active) {
            setBrush(AssterUI::Colors::ButtonActiveBg);
            pRenderTarget->FillRectangle(r, pSolidBrush);
            setBrush(AssterUI::Colors::ButtonActiveText);
            drawText(btn.wText.c_str(), btn.x, btn.y, btn.w, btn.h, pFormatButton);
        } else {
            setBrush(hovered && !disabled ? AssterUI::Colors::ButtonHoverBg : AssterUI::Colors::ButtonInactiveBg);
            pRenderTarget->FillRectangle(r, pSolidBrush);
            setBrush(AssterUI::Colors::ButtonInactiveBorder);
            pRenderTarget->DrawRectangle(r, pSolidBrush, 1.0f);
            setBrush(disabled ? AssterUI::Colors::TextDim : (hovered ? AssterUI::Colors::TextActive : AssterUI::Colors::TextMuted));
            drawText(btn.wText.c_str(), btn.x, btn.y, btn.w, btn.h, pFormatButton);
        }
    }

    void drawKnob(const AssterUI::KnobWidget& k) {
        float val = engine.getParameter(k.paramId);
        if (k.paramId == PARAM_MASTER_PUSH && engine.getParameter(PARAM_TARGET_LUFS_ON) > 0.5f) {
            val = engine.getEffectiveLimiterDriveDb();
        }
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

        drawArc(k.x, k.y, k.radius, radStart, radMax, AssterUI::Colors::KnobTrack, trackStroke);
        if (sweepAngle > 0.001f) {
            drawArc(k.x, k.y, k.radius, radStart, radEnd, AssterUI::Colors::KnobArc, activeStroke);
        }

        float innerR = k.radius - (k.isHero ? 6.5f : 4.5f);
        setBrush(AssterUI::Colors::KnobInner);
        pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(k.x, k.y), innerR, innerR), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(k.x, k.y), innerR, innerR), pSolidBrush, 1.0f);

        float p1x = k.x + (innerR * 0.35f) * std::cos(radEnd);
        float p1y = k.y + (innerR * 0.35f) * std::sin(radEnd);
        float p2x = k.x + (innerR * 0.90f) * std::cos(radEnd);
        float p2y = k.y + (innerR * 0.90f) * std::sin(radEnd);

        setBrush(AssterUI::Colors::TextBright);
        pRenderTarget->DrawLine(D2D1::Point2F(p1x, p1y), D2D1::Point2F(p2x, p2y), pSolidBrush, k.isHero ? 2.5f : 1.8f, pRoundStrokeStyle);

        float titleY = k.y + k.radius + (k.isHero ? 5.0f : 6.0f);
        setBrush(AssterUI::Colors::TextMuted);
        drawText(k.wTitle.c_str(), k.x - k.radius - 26, titleY, (k.radius + 26) * 2, 13, pFormatKnobTitle);

        char valStr[32];
        if (defs[k.paramId].isInt) {
            snprintf(valStr, sizeof(valStr), "%d %s", static_cast<int>(std::round(val)), defs[k.paramId].label);
        } else if (std::abs(val) < 10.0f) {
            snprintf(valStr, sizeof(valStr), "%+.1f %s", val, defs[k.paramId].label);
        } else {
            snprintf(valStr, sizeof(valStr), "%+.0f %s", val, defs[k.paramId].label);
        }
        std::wstring wVal(valStr, valStr + strlen(valStr));

        float boxW = k.isHero ? 78.0f : 58.0f;
        float boxH = k.isHero ? 15.0f : 14.0f;
        float boxX = k.x - boxW * 0.5f;
        float boxY = titleY + (k.isHero ? 13.0f : 12.0f);

        setBrush(AssterUI::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(boxX, boxY, boxX + boxW, boxY + boxH), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(boxX + 0.5f, boxY + 0.5f, boxX + boxW - 0.5f, boxY + boxH - 0.5f), pSolidBrush, 1.0f);

        setBrush(AssterUI::Colors::TextActive);
        drawText(wVal.c_str(), boxX, boxY, boxW, boxH, pFormatKnobVal);
    }

    void drawAssistantModule() {
        float cx = 312.0f, cy = 400.0f, cw = 104.0f, ch = 22.0f;
        bool isLearning = engine.isCalibrating();
        D2D1_RECT_F rc = D2D1::RectF(cx, cy, cx + cw, cy + ch);
        if (isLearning) {
            setBrush(AssterUI::Colors::ButtonActiveBg);
            pRenderTarget->FillRectangle(rc, pSolidBrush);
            setBrush(AssterUI::Colors::ButtonActiveText);
            drawText(L"CALIBRATING", cx, cy, cw, ch, pFormatButton);
        } else {
            setBrush(AssterUI::Colors::ButtonInactiveBg);
            pRenderTarget->FillRectangle(rc, pSolidBrush);
            setBrush(AssterUI::Colors::ButtonInactiveBorder);
            pRenderTarget->DrawRectangle(rc, pSolidBrush, 1.0f);
            setBrush(AssterUI::Colors::TextBright);
            drawText(L"CALIBRATE", cx, cy, cw, ch, pFormatButton);
        }

        float ax = 424.0f, ay = 400.0f, aw = 104.0f, ah = 22.0f;
        bool isAuto = engine.isAutoMastering();
        D2D1_RECT_F ra = D2D1::RectF(ax, ay, ax + aw, ay + ah);
        setBrush(AssterUI::Colors::ButtonInactiveBg);
        pRenderTarget->FillRectangle(ra, pSolidBrush);
        if (isAuto) {
            float prog = engine.getAutoMasterProgress();
            setBrush(AssterUI::Colors::AccentCyan);
            pRenderTarget->FillRectangle(D2D1::RectF(ax, ay, ax + aw * prog, ay + ah), pSolidBrush);
            setBrush(AssterUI::Colors::TextBright);
            drawText(L"ANALYZING...", ax, ay, aw, ah, pFormatButton);
        } else {
            setBrush(AssterUI::Colors::AccentCyan);
            pRenderTarget->DrawRectangle(ra, pSolidBrush, 1.0f);
            setBrush(AssterUI::Colors::TextBright);
            drawText(engine.hasReferenceProfile() ? L"MATCH REF" : L"AUTO-MASTER", ax, ay, aw, ah, pFormatButton);
        }

        float rx = 312.0f, ry = 428.0f, rw = 216.0f, rh = 18.0f;
        setBrush(AssterUI::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(rx, ry, rx + rw, ry + rh), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(rx + 0.5f, ry + 0.5f, rx + rw - 0.5f, ry + rh - 0.5f), pSolidBrush, 1.0f);

        char gainBuf[64];
        snprintf(gainBuf, sizeof(gainBuf), "AutoTrim: %+0.1f dB  |  In: %0.1f LUFS",
                 engine.getAutoTrimGainDb(), engine.getIncomingLUFS());
        std::wstring wGain(gainBuf, gainBuf + strlen(gainBuf));
        setBrush(AssterUI::Colors::TextActive);
        drawText(wGain.c_str(), rx, ry, rw, rh, pFormatKnobVal);

        // Draggable Target LUFS value box (424, 452, 104, 20)
        float tx = 424.0f, ty = 452.0f, tw = 104.0f, th = 20.0f;
        setBrush(AssterUI::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(tx, ty, tx + tw, ty + th), pSolidBrush);
        setBrush(engine.getParameter(PARAM_TARGET_LUFS_ON) > 0.5f ? AssterUI::Colors::AccentCyan : AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(tx + 0.5f, ty + 0.5f, tx + tw - 0.5f, ty + th - 0.5f), pSolidBrush, 1.0f);
        wchar_t tgtStr[32];
        swprintf_s(tgtStr, 32, L"%0.1f LUFS", engine.getParameter(PARAM_TARGET_LUFS));
        setBrush(AssterUI::Colors::TextBright);
        drawText(tgtStr, tx, ty, tw, th, pFormatKnobVal);
    }

    void drawMeteringDeck() {
        float deckX = 16.0f;
        float deckY = 578.0f;
        float deckW = 1088.0f;
        float deckH = 108.0f;

        setBrush(AssterUI::Colors::BgCard);
        pRenderTarget->FillRectangle(D2D1::RectF(deckX, deckY, deckX + deckW, deckY + deckH), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(deckX + 0.5f, deckY + 0.5f, deckX + deckW - 0.5f, deckY + deckH - 0.5f), pSolidBrush, 1.0f);

        setBrush(AssterUI::Colors::TextActive);
        drawText(L"PRECISION METERING DECK", deckX + 12.0f, deckY + 5.0f, 240.0f, 16.0f, pFormatModule);

        float stLUFS  = engine.getOutputShortTermLUFS();
        float intLUFS = engine.getOutputIntegratedLUFS();
        float tpDb    = engine.getOutputTruePeakdBTP();
        float plr     = engine.getPLR();
        float lra     = engine.getLRA();
        float limGR   = engine.getLimiterGR();
        float spotPen = engine.getSpotifyPenaltyDb();
        float compGR  = engine.getGlueGR() + 0.33f * (engine.getMBLowGR() + engine.getMBMidGR() + engine.getMBHighGR());

        struct MetricBox {
            float x;
            const wchar_t* tag;
            wchar_t val[32];
        };

        MetricBox boxes[6];
        for (int i = 0; i < 6; ++i) boxes[i].x = deckX + 12.0f + i * 142.0f;
        boxes[0].tag = L"SHORT-TERM";  swprintf_s(boxes[0].val, 32, L"%0.1f LUFS", stLUFS);
        boxes[1].tag = L"INTEGRATED";  swprintf_s(boxes[1].val, 32, L"%0.1f LUFS", intLUFS);
        boxes[2].tag = L"TRUE PEAK";   swprintf_s(boxes[2].val, 32, L"%0.2f dBTP", tpDb);
        boxes[3].tag = L"PLR / LRA";   swprintf_s(boxes[3].val, 32, L"%0.1f / %0.1f", plr, lra);
        boxes[4].tag = L"LIMITER GR";  swprintf_s(boxes[4].val, 32, L"-%0.1f dB", limGR);
        boxes[5].tag = L"SPOTIFY";     swprintf_s(boxes[5].val, 32, L"%0.1f dB", spotPen);

        for (int i = 0; i < 6; ++i) {
            float bx = boxes[i].x;
            float by = deckY + 24.0f;
            float bw = 134.0f;
            float bh = 38.0f;

            setBrush(AssterUI::Colors::BgHeader);
            pRenderTarget->FillRectangle(D2D1::RectF(bx, by, bx + bw, by + bh), pSolidBrush);
            setBrush(AssterUI::Colors::BorderCard);
            pRenderTarget->DrawRectangle(D2D1::RectF(bx + 0.5f, by + 0.5f, bx + bw - 0.5f, by + bh - 0.5f), pSolidBrush, 1.0f);

            setBrush(AssterUI::Colors::TextMuted);
            drawText(boxes[i].tag, bx, by + 2.0f, bw, 13.0f, pFormatMeterTag);
            setBrush(AssterUI::Colors::TextBright);
            drawText(boxes[i].val, bx, by + 14.0f, bw, 22.0f, pFormatMeterVal);
        }

        float badgeX = deckX + 868.0f;
        float badgeY = deckY + 26.0f;
        float badgeW = 208.0f;
        float badgeH = 34.0f;

        setBrush(AssterUI::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(badgeX, badgeY, badgeX + badgeW, badgeY + badgeH), pSolidBrush);
        setBrush(AssterUI::Colors::TextActive);
        pRenderTarget->DrawRectangle(D2D1::RectF(badgeX + 0.5f, badgeY + 0.5f, badgeX + badgeW - 0.5f, badgeY + badgeH - 0.5f), pSolidBrush, 1.0f);

        float clipAct = engine.getClipActivity();
        setBrush(clipAct > 0.1f ? AssterUI::Colors::AccentAmber : AssterUI::Colors::AccentGreen);
        pRenderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(badgeX + 16.0f, badgeY + badgeH * 0.5f), 4.0f, 4.0f), pSolidBrush);
        setBrush(AssterUI::Colors::TextBright);
        drawText(L"ZERO-CLIP PROTECTED", badgeX + 26.0f, badgeY, badgeW - 26.0f, badgeH, pFormatButton);

        float meterX = deckX + 12.0f;
        float meterY = deckY + 72.0f;
        float meterW = 846.0f;
        float meterH = 12.0f;

        setBrush(AssterUI::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(meterX, meterY, meterX + meterW, meterY + meterH), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(meterX + 0.5f, meterY + 0.5f, meterX + meterW - 0.5f, meterY + meterH - 0.5f), pSolidBrush, 1.0f);

        float totalGR = limGR + compGR;
        float grNorm = std::clamp(totalGR / 12.0f, 0.0f, 1.0f);
        float fillW = grNorm * meterW;
        if (fillW > 1.0f) {
            setBrush(AssterUI::Colors::AccentRed);
            pRenderTarget->FillRectangle(D2D1::RectF(meterX, meterY, meterX + fillW, meterY + meterH), pSolidBrush);
        }

        const wchar_t* ticks[5] = { L"0 dB", L"-3 dB", L"-6 dB", L"-9 dB", L"-12 dB" };
        setBrush(AssterUI::Colors::TextMuted);
        for (int t = 0; t < 5; ++t) {
            float tx = meterX + (meterW * 0.25f * t);
            drawText(ticks[t], tx - 22.0f, meterY + 14.0f, 44.0f, 12.0f, pFormatSmall);
        }
        drawText(L"TOTAL GAIN REDUCTION", meterX + meterW + 14.0f, meterY - 1.0f, 200.0f, 14.0f, pFormatKnobTitle);
    }

    void drawHelpBar() {
        setBrush(AssterUI::Colors::BgHeader);
        pRenderTarget->FillRectangle(D2D1::RectF(16.0f, 692.0f, 1104.0f, 712.0f), pSolidBrush);
        setBrush(AssterUI::Colors::BorderCard);
        pRenderTarget->DrawRectangle(D2D1::RectF(16.5f, 692.5f, 1103.5f, 711.5f), pSolidBrush, 1.0f);
        std::string diag = engine.getDiagnosticSummary();
        if (isDefaultHelp && !diag.empty()) {
            std::wstring wDiag(diag.begin(), diag.end());
            setBrush(AssterUI::Colors::AccentCyan);
            drawText(wDiag.c_str(), 24.0f, 692.0f, 1072.0f, 20.0f, pFormatSmall);
        } else {
            setBrush(AssterUI::Colors::TextMuted);
            drawText(hoverHelp, 24.0f, 692.0f, 1072.0f, 20.0f, pFormatSmall);
        }
    }

    void drawArc(float cx, float cy, float r, float startAngleRad, float endAngleRad, const AssterUI::D2DColor& color, float strokeWidth) {
        if (!pRenderTarget) return;
        float diff = endAngleRad - startAngleRad;
        if (std::abs(diff) < 0.0001f) return;

        setBrush(color);
        constexpr int SEGMENTS = 22;
        float step = diff / static_cast<float>(SEGMENTS);
        float a = startAngleRad;
        D2D1_POINT_2F prev = D2D1::Point2F(cx + r * std::cos(a), cy + r * std::sin(a));
        for (int i = 1; i <= SEGMENTS; ++i) {
            a += step;
            D2D1_POINT_2F next = D2D1::Point2F(cx + r * std::cos(a), cy + r * std::sin(a));
            pRenderTarget->DrawLine(prev, next, pSolidBrush, strokeWidth, pRoundStrokeStyle);
            prev = next;
        }
    }

    void drawText(const wchar_t* str, float x, float y, float w, float h, IDWriteTextFormat* pFmt) {
        if (pRenderTarget && pSolidBrush && pFmt && str) {
            pRenderTarget->DrawText(str, static_cast<UINT32>(wcslen(str)), pFmt, D2D1::RectF(x, y, x + w, y + h), pSolidBrush);
        }
    }

    void setBrush(const AssterUI::D2DColor& c) {
        if (pSolidBrush) pSolidBrush->SetColor(c.toD2D());
    }
};

} // namespace Asster
#endif
