#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include "../dsp/AssterDSP.hpp"
#include "../gui/WinMasteringUI.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Create top-level host window
    const wchar_t* HOST_CLASS = L"AssterStandaloneHostClass";
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = HOST_CLASS;
    RegisterClassExW(&wc);

    RECT r = { 0, 0, Asster::WinMasteringUI::UI_WIDTH, Asster::WinMasteringUI::UI_HEIGHT };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int winW = r.right - r.left;
    int winH = r.bottom - r.top;

    HWND hostHwnd = CreateWindowExW(
        0, HOST_CLASS,
        L"Mastering Processor — Standalone",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winW, winH,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hostHwnd) return 1;

    Asster::AssterEngine engine;
    engine.setSampleRate(44100.0f);
    engine.loadPreset(0);

    auto ui = std::make_unique<Asster::WinMasteringUI>(engine);
    ui->open(hostHwnd);

    ShowWindow(hostHwnd, nCmdShow);
    UpdateWindow(hostHwnd);

    // Background thread feeding synthetic metal audio to drive live metering
    std::atomic<bool> audioRunning{true};
    std::thread audioThread([&engine, &audioRunning]() {
        float sampleRate = 44100.0f;
        int blockSize = 512;
        std::vector<float> inL(blockSize);
        std::vector<float> inR(blockSize);
        std::vector<float> outL(blockSize);
        std::vector<float> outR(blockSize);

        float phase = 0.0f;
        while (audioRunning) {
            for (int i = 0; i < blockSize; ++i) {
                float t = phase / sampleRate;
                float sub = 0.4f * std::sin(2.0f * (float)M_PI * 55.0f * t);
                float kick = (std::fmod(t, 0.4f) < 0.06f) ? 0.9f * std::exp(-std::fmod(t, 0.4f) * 45.0f) : 0.0f;
                float chug = 0.35f * std::sin(2.0f * (float)M_PI * 110.0f * t) * (std::sin(2.0f * (float)M_PI * 220.0f * t) > 0.0f ? 1.0f : -1.0f);
                float noise = 0.15f * (((rand() % 1000) / 500.0f) - 1.0f);

                inL[i] = (sub + kick + chug + noise) * 0.7f;
                inR[i] = (sub + kick + chug * 0.9f - noise) * 0.7f;
                phase += 1.0f;
            }

            float* inPtrs[2] = { inL.data(), inR.data() };
            float* outPtrs[2] = { outL.data(), outR.data() };
            engine.process(inPtrs, outPtrs, blockSize);

            std::this_thread::sleep_for(std::chrono::milliseconds(11));
        }
    });

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(hostHwnd)) break;
    }

    audioRunning = false;
    if (audioThread.joinable()) audioThread.join();

    ui->close();
    ui.reset();
    CoUninitialize();
    return 0;
}

