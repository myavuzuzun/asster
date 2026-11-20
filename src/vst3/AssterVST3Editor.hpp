#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include "../../include/vst3_plugin.h"
#include "../gui/WinMasteringUI.hpp"

class AssterVST3EditorWrapper : public VST3_Editor {
public:
    AssterVST3EditorWrapper(VST3_EditorListener* listener, VST3_Descriptor* desc, void* parent, Asster::AssterEngine* eng)
        : VST3_Editor(listener, desc, parent), engine(eng) {
        gui = std::make_unique<Asster::WinMasteringUI>(*eng);
        gui->open(static_cast<HWND>(parent));
    }

    ~AssterVST3EditorWrapper() override {
        gui.reset();
    }

    void updateParameterFromHost(uint32_t /*index*/, float /*value*/) override {
        if (gui) {
            gui->redraw();
        }
    }

    Asster::WinMasteringUI* getUI() { return gui.get(); }

private:
    Asster::AssterEngine* engine = nullptr;
    std::unique_ptr<Asster::WinMasteringUI> gui;
};

