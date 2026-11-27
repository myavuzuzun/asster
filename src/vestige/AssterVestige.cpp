#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <memory>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "../gui/WinMasteringUI.hpp"

// Open-source VeSTige header (Javier Serrano Polo / LMMS)
#include "../../include/vestige/vestige.h"
#include "../dsp/AssterDSP.hpp"

// VeSTige supplementary opcodes and flags
#ifndef effSetProgramName
#define effSetProgramName 4
#endif
#ifndef effGetParamLabel
#define effGetParamLabel 6
#endif
#ifndef effGetParamDisplay
#define effGetParamDisplay 7
#endif
#ifndef effGetChunk
#define effGetChunk 23
#endif
#ifndef effSetChunk
#define effSetChunk 24
#endif
#ifndef effGetProgramNameIndexed
#define effGetProgramNameIndexed 29
#endif
#ifndef effGetTailSize
#define effGetTailSize 52
#endif
#ifndef effFlagsProgramChunks
#define effFlagsProgramChunks (1 << 5)
#endif
#ifndef effFlagsCanDoubleReplacing
#define effFlagsCanDoubleReplacing (1 << 12)
#endif

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#include <pmmintrin.h>

struct ScopedFtzDaz {
    unsigned int oldMxcsr;
    ScopedFtzDaz() {
        oldMxcsr = _mm_getcsr();
        // 0x8000 = Flush-to-Zero (FTZ), 0x0040 = Denormals-are-Zero (DAZ)
        _mm_setcsr(oldMxcsr | 0x8040);
    }
    ~ScopedFtzDaz() {
        _mm_setcsr(oldMxcsr);
    }
};
#else
struct ScopedFtzDaz {};
#endif

struct ERect {
    int16_t top;
    int16_t left;
    int16_t bottom;
    int16_t right;
};

struct AssterEffectBlock {
    AEffect fx;
    void (__cdecl *processDoubleReplacing)(AEffect*, double**, double**, int);
    char future[56];
};

class AssterVestigeWrapper {
public:
    AssterVestigeWrapper(audioMasterCallback master) : audioMaster(master) {
        (void)this->audioMaster;
        std::memset(&effectBlock, 0, sizeof(AssterEffectBlock));
        AEffect& fx = effectBlock.fx;
        fx.magic = kEffectMagic;
        fx.dispatcher = dispatcherCallback;
        fx.process = nullptr;
        fx.processReplacing = processReplacingCallback;
        effectBlock.processDoubleReplacing = processDoubleReplacingCallback;
        fx.setParameter = setParameterCallback;
        fx.getParameter = getParameterCallback;
        fx.numPrograms = static_cast<int>(Asster::getMetalPresets().size());
        fx.numParams = Asster::NUM_PARAMS;
        fx.numInputs = 2;
        fx.numOutputs = 2;
        fx.flags = effFlagsHasEditor | effFlagsCanReplacing | effFlagsCanDoubleReplacing | effFlagsProgramChunks;
        fx.uniqueID = CCONST('A', 'S', 'S', 'T');
        fx.version = 1000;
        fx.object = this;
        fx.initialDelay = engine.getLatencySamples();

        editorRect.top = 0;
        editorRect.left = 0;
        editorRect.bottom = Asster::WinMasteringUI::UI_HEIGHT;
        editorRect.right = Asster::WinMasteringUI::UI_WIDTH;

        currentProgram = 0;
        engine.setSampleRate(44100.0f);
    }

    ~AssterVestigeWrapper() {
        closeEditor();
    }

    AEffect* getAEffect() { return &effectBlock.fx; }

    static intptr_t __cdecl dispatcherCallback(AEffect* effect, int opcode, int index, intptr_t value, void* ptr, float opt) {
        if (!effect || !effect->object) return 0;
        auto* self = static_cast<AssterVestigeWrapper*>(effect->object);
        return self->dispatcher(opcode, index, value, ptr, opt);
    }

    static void __cdecl processReplacingCallback(AEffect* effect, float** inputs, float** outputs, int sampleFrames) {
        if (!effect || !effect->object) return;
        ScopedFtzDaz ftzDaz;
        auto* self = static_cast<AssterVestigeWrapper*>(effect->object);
        self->engine.process(inputs, outputs, sampleFrames);
    }

    static void __cdecl processDoubleReplacingCallback(AEffect* effect, double** inputs, double** outputs, int sampleFrames) {
        if (!effect || !effect->object) return;
        ScopedFtzDaz ftzDaz;
        auto* self = static_cast<AssterVestigeWrapper*>(effect->object);
        
        thread_local std::vector<float> inBufL, inBufR, outBufL, outBufR;
        if ((int)inBufL.capacity() < 4096) {
            inBufL.reserve(4096);
            inBufR.reserve(4096);
            outBufL.reserve(4096);
            outBufR.reserve(4096);
        }
        if ((int)inBufL.size() < sampleFrames) {
            inBufL.resize(sampleFrames);
            inBufR.resize(sampleFrames);
            outBufL.resize(sampleFrames);
            outBufR.resize(sampleFrames);
        }

        for (int i = 0; i < sampleFrames; ++i) {
            inBufL[i] = static_cast<float>(inputs[0][i]);
            inBufR[i] = (inputs[1] != nullptr) ? static_cast<float>(inputs[1][i]) : inBufL[i];
        }

        const float* inPtrs[2] = { inBufL.data(), inBufR.data() };
        float* outPtrs[2] = { outBufL.data(), outBufR.data() };

        self->engine.process(inPtrs, outPtrs, sampleFrames);

        for (int i = 0; i < sampleFrames; ++i) {
            outputs[0][i] = static_cast<double>(outBufL[i]);
            if (outputs[1]) outputs[1][i] = static_cast<double>(outBufR[i]);
        }
    }

    static void __cdecl setParameterCallback(AEffect* effect, int index, float parameter) {
        if (!effect || !effect->object) return;
        auto* self = static_cast<AssterVestigeWrapper*>(effect->object);
        if (index < 0 || index >= Asster::NUM_PARAMS) return;

        const auto* defs = Asster::getParameterDefs();
        float denorm = defs[index].minVal + parameter * (defs[index].maxVal - defs[index].minVal);
        self->engine.setParameter(index, denorm);
        self->effectBlock.fx.initialDelay = self->engine.getLatencySamples();

        if (self->ui) {
            self->ui->redraw();
        }
    }

    static float __cdecl getParameterCallback(AEffect* effect, int index) {
        if (!effect || !effect->object) return 0.0f;
        auto* self = static_cast<AssterVestigeWrapper*>(effect->object);
        if (index < 0 || index >= Asster::NUM_PARAMS) return 0.0f;

        const auto* defs = Asster::getParameterDefs();
        float val = self->engine.getParameter(index);
        float range = defs[index].maxVal - defs[index].minVal;
        return (range > 0.0f) ? ((val - defs[index].minVal) / range) : 0.0f;
    }

    intptr_t dispatcher(int opcode, int index, intptr_t value, void* ptr, float opt) {
        (void)opt;
        const auto* defs = Asster::getParameterDefs();
        const auto& presets = Asster::getMetalPresets();

        switch (opcode) {
            case effOpen:
                return 0;

            case effClose:
                delete this;
                return 0;

            case effEditGetRect:
                if (ptr) {
                    *reinterpret_cast<ERect**>(ptr) = &editorRect;
                    return 1;
                }
                return 0;

            case effEditOpen:
                if (ptr) {
                    openEditor(ptr);
                    return 1;
                }
                return 0;

            case effEditClose:
                closeEditor();
                return 1;

            case effEditIdle: // Opcode 19 (editor idle refresh)
                if (ui) {
                    ui->idle();
                }
                return 1;

            case effSetProgram:
                if (value >= 0 && value < static_cast<intptr_t>(presets.size())) {
                    currentProgram = static_cast<int32_t>(value);
                    engine.loadPreset(currentProgram);
                    if (ui) ui->redraw();
                    return 1;
                }
                return 0;

            case effGetChunk:
                if (ptr) {
                    engine.serializeState(chunkBuffer);
                    *reinterpret_cast<void**>(ptr) = chunkBuffer.data();
                    return static_cast<intptr_t>(chunkBuffer.size());
                }
                return 0;

            case effSetChunk:
                if (ptr && value > 0) {
                    if (engine.deserializeState(ptr, static_cast<size_t>(value))) {
                        effectBlock.fx.initialDelay = engine.getLatencySamples();
                        if (ui) ui->redraw();
                        return 1;
                    }
                }
                return 0;

            case effGetProgram:
                return currentProgram;

            case effSetProgramName:
                return 1;

            case effGetProgramName:
                if (ptr && currentProgram >= 0 && currentProgram < static_cast<int32_t>(presets.size())) {
                    std::strncpy(static_cast<char*>(ptr), presets[currentProgram].name, 24);
                    static_cast<char*>(ptr)[23] = '\0';
                    return 1;
                }
                return 0;

            case effGetProgramNameIndexed:
                if (ptr && index >= 0 && index < static_cast<int32_t>(presets.size())) {
                    std::strncpy(static_cast<char*>(ptr), presets[index].name, 24);
                    static_cast<char*>(ptr)[23] = '\0';
                    return 1;
                }
                return 0;

            case effGetParamLabel:
                if (ptr && index >= 0 && index < Asster::NUM_PARAMS) {
                    std::strncpy(static_cast<char*>(ptr), defs[index].label, 8);
                    static_cast<char*>(ptr)[7] = '\0';
                    return 1;
                }
                return 0;

            case effGetParamDisplay:
                if (ptr && index >= 0 && index < Asster::NUM_PARAMS) {
                    float val = engine.getParameter(index);
                    if (defs[index].isInt) {
                        std::snprintf(static_cast<char*>(ptr), 16, "%d", static_cast<int>(val));
                    } else {
                        std::snprintf(static_cast<char*>(ptr), 16, "%.2f", val);
                    }
                    return 1;
                }
                return 0;

            case effGetParamName:
                if (ptr && index >= 0 && index < Asster::NUM_PARAMS) {
                    std::strncpy(static_cast<char*>(ptr), defs[index].name, 24);
                    static_cast<char*>(ptr)[23] = '\0';
                    return 1;
                }
                return 0;

            case effSetSampleRate:
                engine.setSampleRate(opt > 1000.0f ? opt : static_cast<float>(value));
                effectBlock.fx.initialDelay = engine.getLatencySamples();
                return 1;

            case effSetBlockSize:
                return 1;

            case effMainsChanged:
                if (value == 0) {
                    engine.reset();
                }
                return 1;

            case effGetEffectName:
                if (ptr) {
                    std::strncpy(static_cast<char*>(ptr), "Asster", 32);
                    return 1;
                }
                return 0;

            case effGetVendorString:
                if (ptr) {
                    std::strncpy(static_cast<char*>(ptr), "myavuzuzun", 32);
                    return 1;
                }
                return 0;

            case effGetProductString:
                if (ptr) {
                    std::strncpy(static_cast<char*>(ptr), "Asster Mastering Tool", 32);
                    return 1;
                }
                return 0;

            case effGetVendorVersion:
                return 1000;

            case effGetPlugCategory:
                return kPlugCategMastering;

            case effCanDo:
                if (ptr) {
                    const char* canDoStr = static_cast<const char*>(ptr);
                    if (std::strcmp(canDoStr, "receiveVstEvents") == 0) return 1;
                    if (std::strcmp(canDoStr, "sendVstEvents") == 0) return 1;
                    if (std::strcmp(canDoStr, "1in2out") == 0) return 1;
                    if (std::strcmp(canDoStr, "2in2out") == 0) return 1;
                }
                return 0;

            case effGetTailSize:
                return 0;

            case effGetVstVersion:
                return 2400;

            default:
                return 0;
        }
    }

    void openEditor(void* parentHandle) {
        closeEditor();
        HWND parentHwnd = reinterpret_cast<HWND>(parentHandle);
        ui = std::make_unique<Asster::WinMasteringUI>(engine);
        ui->open(parentHwnd);
    }

    void closeEditor() {
        if (ui) {
            ui->close();
            ui.reset();
        }
    }

private:
    audioMasterCallback audioMaster;
    AssterEffectBlock effectBlock;
    ERect editorRect;
    int32_t currentProgram;
    Asster::AssterEngine engine;
    std::vector<uint8_t> chunkBuffer;
    std::unique_ptr<Asster::WinMasteringUI> ui;
};

extern "C" BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)hinstDLL;
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

extern "C" {

#define VST_EXPORT __declspec(dllexport)

VST_EXPORT AEffect* VSTPluginMain(audioMasterCallback audioMaster) {
    if (!audioMaster) return nullptr;
    auto* wrapper = new AssterVestigeWrapper(audioMaster);
    return wrapper->getAEffect();
}

VST_EXPORT AEffect* main_plugin(audioMasterCallback audioMaster) asm("main");
VST_EXPORT AEffect* main_plugin(audioMasterCallback audioMaster) {
    return VSTPluginMain(audioMaster);
}

}
