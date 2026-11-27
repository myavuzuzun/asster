#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
#include <xmmintrin.h>
#include <pmmintrin.h>

struct ScopedFtzDaz {
    unsigned int oldMxcsr;
    ScopedFtzDaz() {
        oldMxcsr = _mm_getcsr();
        _mm_setcsr(oldMxcsr | 0x8040); // 0x8000 = FTZ, 0x0040 = DAZ
    }
    ~ScopedFtzDaz() {
        _mm_setcsr(oldMxcsr);
    }
};
#else
struct ScopedFtzDaz {};
#endif

#include "../../include/vst3_plugin.h"
#include "../dsp/AssterDSP.hpp"
#include "../gui/WinMasteringUI.hpp"
#include "AssterVST3Editor.hpp"

class AssterParameter : public VST3_Parameter {
public:
    AssterParameter(int id, const char* nameStr, const char* labelStr, float defVal01, float minV, float maxV, bool isInteger)
        : VST3_Parameter(nullptr, defVal01), paramId(id), minVal(minV), maxVal(maxV), isInt(isInteger) {
        std::memset(paramName, 0, sizeof(paramName));
        std::strncpy(paramName, nameStr, sizeof(paramName) - 1);
        name = paramName;
        short_name = paramName;
        label = labelStr;
        if (isInt) {
            num_steps = static_cast<uint32_t>(maxVal - minVal);
        }
    }

    float from01(float val01) override {
        return minVal + val01 * (maxVal - minVal);
    }

    float to01(float val) override {
        float range = maxVal - minVal;
        return (range > 0.0f) ? std::clamp((val - minVal) / range, 0.0f, 1.0f) : 0.0f;
    }

    const char* displayText(float val01, char* buf) override {
        float val = from01(val01);
        if (isInt) {
            std::snprintf(buf, 32, "%d", static_cast<int>(std::round(val)));
        } else if (std::abs(val) < 10.0f) {
            std::snprintf(buf, 32, "%+.1f", val);
        } else {
            std::snprintf(buf, 32, "%+.0f", val);
        }
        return buf;
    }

private:
    int paramId;
    float minVal;
    float maxVal;
    bool isInt;
    char paramName[64];
};

class AssterDescriptor : public VST3_Descriptor {
public:
    AssterDescriptor() {
        name = "Asster";
        author = "myavuzuzun";
        url = "https://github.com/myavuzuzun";
        email = "myavuzuzun@example.com";
        version_text = "1.0.0";
        version = 1000;
        is_synth = false;
        has_editor = true;
        can_send_midi = false;
        can_receive_midi = false;
        editor_width = Asster::WinMasteringUI::UI_WIDTH;
        editor_height = Asster::WinMasteringUI::UI_HEIGHT;
        num_inputs = 2;
        num_outputs = 2;

        static const uint8_t p_id[16] = { 0x41, 0x53, 0x53, 0x54, 0x00, 0x01, 0x4D, 0x59, 0x55, 0x5A, 0x55, 0x4E, 0x4D, 0x53, 0x54, 0x31 };
        static const uint8_t e_id[16] = { 0x41, 0x53, 0x53, 0x54, 0x00, 0x02, 0x4D, 0x59, 0x55, 0x5A, 0x55, 0x4E, 0x4D, 0x53, 0x54, 0x32 };
        std::memcpy(const_cast<uint8_t*>(plugin_id), p_id, 16);
        std::memcpy(const_cast<uint8_t*>(editor_id), e_id, 16);

        const auto* defs = Asster::getParameterDefs();
        for (int i = 0; i < Asster::NUM_PARAMS; ++i) {
            float range = defs[i].maxVal - defs[i].minVal;
            float def01 = (range > 0.0f) ? ((defs[i].defaultVal - defs[i].minVal) / range) : 0.0f;
            appendParameter(new AssterParameter(i, defs[i].name, defs[i].label, def01, defs[i].minVal, defs[i].maxVal, defs[i].isInt));
        }
    }
};

class AssterInstance : public VST3_Instance {
public:
    AssterInstance() {
        engine.setSampleRate(44100.0f);
        engine.loadPreset(0);
    }
    ~AssterInstance() override = default;

    void on_prepare(float sampleRate) override {
        engine.setSampleRate(sampleRate);
    }

    uint32_t on_getLatencySamples() override {
        return static_cast<uint32_t>(engine.getLatencySamples());
    }

    uint32_t on_saveState(void** ABuffer, uint32_t /*AMode*/) override {
        engine.serializeState(savedStateBuf);
        *ABuffer = savedStateBuf.data();
        return static_cast<uint32_t>(savedStateBuf.size());
    }

    void on_restoreState(uint32_t ASize, void* APointer, uint32_t /*AMode*/) override {
        if (engine.deserializeState(APointer, ASize)) {
            if (activeEditor) {
                activeEditor->updateParameterFromHost(0, 0.0f);
            }
        }
    }

    void on_parameter(uint32_t index, float value, uint32_t /*mode*/) override {
        if (index < Asster::NUM_PARAMS) {
            engine.setParameter(index, value);
            if (activeEditor) {
                const auto* defs = Asster::getParameterDefs();
                float range = defs[index].maxVal - defs[index].minVal;
                float norm = (range > 0.0f) ? std::clamp((value - defs[index].minVal) / range, 0.0f, 1.0f) : 0.0f;
                activeEditor->updateParameterFromHost(index, norm);
            }
        }
    }

    void on_process(VST3_ProcessContext* ctx) override {
        ScopedFtzDaz ftz;
        if (ctx->num_samples > 0 && ctx->inputs[0] && ctx->outputs[0]) {
            float* inR = ctx->inputs[1] ? ctx->inputs[1] : ctx->inputs[0];
            float* ins[2] = { ctx->inputs[0], inR };
            float* outR = ctx->outputs[1] ? ctx->outputs[1] : ctx->outputs[0];
            float* outs[2] = { ctx->outputs[0], outR };
            engine.process(ins, outs, ctx->num_samples);
        } else if (ctx->num_samples > 0) {
            if (ctx->outputs[0]) std::memset(ctx->outputs[0], 0, sizeof(float) * ctx->num_samples);
            if (ctx->outputs[1]) std::memset(ctx->outputs[1], 0, sizeof(float) * ctx->num_samples);
        }
    }

    VST3_Editor* on_openEditor(void* parent) override {
        activeEditor = new AssterVST3EditorWrapper(this, MDescriptor, parent, &engine);
        return activeEditor;
    }

    void on_closeEditor(VST3_Editor* editor) override {
        if (activeEditor == editor) {
            activeEditor = nullptr;
        }
        delete editor;
    }

private:
    Asster::AssterEngine engine;
    AssterVST3EditorWrapper* activeEditor = nullptr;
    std::vector<uint8_t> savedStateBuf;
};

VST3_ENTRYPOINT(AssterDescriptor, AssterInstance)

