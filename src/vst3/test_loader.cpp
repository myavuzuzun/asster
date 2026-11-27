#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <cmath>
#include "../../include/vst3_plugin.h"

class MockEventList : public IEventList {
public:
    std::vector<Event> events;

    tresult PLUGIN_API queryInterface(const FUID /*_iid*/, void** obj) override {
        *obj = nullptr;
        return kResultFalse;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }

    int32 PLUGIN_API getEventCount() override {
        return static_cast<int32>(events.size());
    }
    tresult PLUGIN_API getEvent(int32 index, Event& e) override {
        if (index >= 0 && index < static_cast<int32>(events.size())) {
            e = events[index];
            return kResultOk;
        }
        return kResultFalse;
    }
    tresult PLUGIN_API addEvent(Event& e) override {
        events.push_back(e);
        return kResultOk;
    }
};

class MockBStream : public IBStream {
public:
    std::vector<uint8_t> buffer;
    int64_t cursor = 0;

    tresult PLUGIN_API queryInterface(const FUID /*_iid*/, void** obj) override {
        *obj = nullptr;
        return kResultFalse;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }

    tresult PLUGIN_API read(void* buf, int32_t numBytes, int32_t* numBytesRead) override {
        int64_t avail = static_cast<int64_t>(buffer.size()) - cursor;
        if (avail <= 0) {
            if (numBytesRead) *numBytesRead = 0;
            return kResultFalse;
        }
        int32_t toRead = static_cast<int32_t>(avail < numBytes ? avail : numBytes);
        std::memcpy(buf, buffer.data() + cursor, toRead);
        cursor += toRead;
        if (numBytesRead) *numBytesRead = toRead;
        return kResultOk;
    }

    tresult PLUGIN_API write(void* buf, int32_t numBytes, int32_t* numBytesWritten) override {
        const uint8_t* src = static_cast<const uint8_t*>(buf);
        buffer.insert(buffer.end(), src, src + numBytes);
        if (numBytesWritten) *numBytesWritten = numBytes;
        return kResultOk;
    }

    tresult PLUGIN_API seek(int64_t pos, int32_t mode, int64_t* result) override {
        if (mode == kIBSeekSet) cursor = pos;
        else if (mode == kIBSeekCur) cursor += pos;
        else if (mode == kIBSeekEnd) cursor = static_cast<int64_t>(buffer.size()) + pos;
        if (result) *result = cursor;
        return kResultOk;
    }

    tresult PLUGIN_API tell(int64_t* pos) override {
        if (pos) *pos = cursor;
        return kResultOk;
    }
};

int main(int argc, char* argv[]) {
    const char* dllPath = (argc > 1) ? argv[1] : "dist/Asster.vst3";
    printf("Loading: %s\n", dllPath);

    HMODULE hMod = LoadLibraryA(dllPath);
    if (!hMod) {
        printf("ERROR: LoadLibrary failed, error code %lu\n", GetLastError());
        return 1;
    }

    typedef bool (__stdcall *InitDllFunc)();
    typedef bool (__stdcall *ExitDllFunc)();
    typedef IPluginFactory* (__stdcall *GetPluginFactoryFunc)();

    InitDllFunc initDll = (InitDllFunc)GetProcAddress(hMod, "InitDll");
    ExitDllFunc exitDll = (ExitDllFunc)GetProcAddress(hMod, "ExitDll");
    GetPluginFactoryFunc getFactory = (GetPluginFactoryFunc)GetProcAddress(hMod, "GetPluginFactory");

    if (!initDll || !exitDll || !getFactory) {
        printf("ERROR: Missing exports (initDll=%p, exitDll=%p, getFactory=%p)\n", initDll, exitDll, getFactory);
        FreeLibrary(hMod);
        return 1;
    }

    initDll();
    IPluginFactory* factory = getFactory();
    if (!factory) {
        printf("ERROR: getFactory returned null\n");
        FreeLibrary(hMod);
        return 1;
    }

    int32 count = factory->countClasses();
    printf("Class count: %d\n", count);

    PClassInfo classInfo;
    std::memset(&classInfo, 0, sizeof(classInfo));
    factory->getClassInfo(0, &classInfo);
    printf("Plugin Name: '%s', Category: '%s'\n", classInfo.name, classInfo.category);

    // Create Instance as IComponent
    void* compObj = nullptr;
    tresult res = factory->createInstance(classInfo.cid, IComponent_iid, &compObj);
    printf("createInstance (IComponent) result: %d, obj=%p\n", res, compObj);

    if (res != kResultOk || !compObj) {
        printf("ERROR: Failed to create IComponent instance!\n");
        FreeLibrary(hMod);
        return 1;
    }

    IComponent* component = (IComponent*)compObj;
    component->initialize(nullptr);

    // Verify Bus Info
    int32 audioIns = component->getBusCount(kAudio, kInput);
    int32 audioOuts = component->getBusCount(kAudio, kOutput);
    int32 eventIns = component->getBusCount(kEvent, kInput);
    int32 eventOuts = component->getBusCount(kEvent, kOutput);
    printf("Buses: Audio In=%d, Audio Out=%d, Event In=%d, Event Out=%d\n", audioIns, audioOuts, eventIns, eventOuts);

    if (audioIns > 0) {
        BusInfo bInfo{};
        component->getBusInfo(kAudio, kInput, 0, bInfo);
        char bname[128] = {0};
        utf16_to_char(bInfo.name, bname);
        printf("  Audio In 0: '%s', busType=%d, flags=0x%X (defaultActive=%s)\n",
               bname, bInfo.busType, bInfo.flags, (bInfo.flags & BusInfo::kDefaultActive) ? "YES" : "NO");
    }
    if (audioOuts > 0) {
        BusInfo bInfo{};
        component->getBusInfo(kAudio, kOutput, 0, bInfo);
        char bname[128] = {0};
        utf16_to_char(bInfo.name, bname);
        printf("  Audio Out 0: '%s', busType=%d, flags=0x%X (defaultActive=%s)\n",
               bname, bInfo.busType, bInfo.flags, (bInfo.flags & BusInfo::kDefaultActive) ? "YES" : "NO");
    }
    if (eventOuts > 0) {
        BusInfo bInfo{};
        component->getBusInfo(kEvent, kOutput, 0, bInfo);
        char bname[128] = {0};
        utf16_to_char(bInfo.name, bname);
        printf("  Event Out 0: '%s', busType=%d, flags=0x%X (defaultActive=%s)\n",
               bname, bInfo.busType, bInfo.flags, (bInfo.flags & BusInfo::kDefaultActive) ? "YES" : "NO");
    }

    BusInfo oobInfo{};
    tresult oobAudioRes = component->getBusInfo(kAudio, kInput, 1, oobInfo);
    tresult oobEventRes = component->getBusInfo(kEvent, kOutput, 1, oobInfo);
    printf("  OOB Bus Check (index=1): audioRes=%d, eventRes=%d\n", oobAudioRes, oobEventRes);
    if (oobAudioRes == kResultOk || oobEventRes == kResultOk) {
        printf("ERROR: getBusInfo must return error for invalid bus index!\n");
        return 1;
    }

    // Query IAudioProcessor
    void* procObj = nullptr;
    res = component->queryInterface(IAudioProcessor_iid, &procObj);
    printf("queryInterface (IAudioProcessor) result: %d, obj=%p\n", res, procObj);

    if (res == kResultOk && procObj) {
        IAudioProcessor* processor = (IAudioProcessor*)procObj;
        ProcessSetup setup;
        setup.processMode = 0;
        setup.symbolicSampleSize = kSample32;
        setup.maxSamplesPerBlock = 512;
        setup.sampleRate = 44100.0;
        processor->setupProcessing(setup);
        processor->setProcessing(true);

        SpeakerArrangement inArr = 0, outArr = 0;
        tresult inArrRes = processor->getBusArrangement(kInput, 0, inArr);
        tresult outArrRes = processor->getBusArrangement(kOutput, 0, outArr);
        printf("Bus Arrangements: inRes=%d (arr=0x%llX), outRes=%d (arr=0x%llX)\n", inArrRes, inArr, outArrRes, outArr);

        // Process a dummy block
        float inL[512] = {0.5f};
        float inR[512] = {-0.5f};
        float outL[512] = {0};
        float outR[512] = {0};
        float* inChs[2] = { inL, inR };
        float* outChs[2] = { outL, outR };

        AudioBusBuffers inBus;
        inBus.numChannels = 2;
        inBus.channelBuffers32 = inChs;
        inBus.silenceFlags = 0;

        AudioBusBuffers outBus;
        outBus.numChannels = 2;
        outBus.channelBuffers32 = outChs;
        outBus.silenceFlags = 0;

        ProcessContext procCtx;
        std::memset(&procCtx, 0, sizeof(procCtx));
        procCtx.sampleRate = 44100.0;
        procCtx.tempo = 120.0;
        procCtx.state = ProcessContext::kPlaying;

        MockEventList mockInEvents;
        MockEventList mockOutEvents;

        Event noteOnEv{};
        noteOnEv.type = Event::kNoteOnEvent;
        noteOnEv.busIndex = 0;
        noteOnEv.sampleOffset = 0;
        noteOnEv.noteOn.channel = 0;
        noteOnEv.noteOn.pitch = 60;
        noteOnEv.noteOn.velocity = 0.8f;
        noteOnEv.noteOn.tuning = 0.0f;
        noteOnEv.noteOn.length = -1;
        noteOnEv.noteOn.noteId = -1;
        mockInEvents.addEvent(noteOnEv);

        ProcessData data;
        data.processMode = 0;
        data.symbolicSampleSize = kSample32;
        data.numSamples = 512;
        data.numInputs = audioIns > 0 ? 1 : 0;
        data.numOutputs = audioOuts > 0 ? 1 : 0;
        data.inputs = audioIns > 0 ? &inBus : nullptr;
        data.outputs = audioOuts > 0 ? &outBus : nullptr;
        data.inputEvents = (eventIns > 0) ? &mockInEvents : nullptr;
        data.outputEvents = (eventOuts > 0) ? &mockOutEvents : nullptr;
        data.inputParameterChanges = nullptr;
        data.outputParameterChanges = nullptr;
        data.processContext = &procCtx;

        res = processor->process(data);
        printf("processor->process result: %d (outL[0]=%.3f, outR[0]=%.3f)\n", res, outL[0], outR[0]);
        if (eventOuts > 0) {
            printf("  Dispatched output events count: %d\n", mockOutEvents.getEventCount());
        }

        // If this is an instrument with SoundFont support, test loading SF2 and note playback
        if (std::string(classInfo.name).find("SF2") != std::string::npos) {
            const char* sfPath = "C:\\Users\\Yavuz\\Documents\\lmms\\samples\\soundfonts\\MS_Basic.sf2";
            FILE* f = fopen(sfPath, "rb");
            if (f) {
                fclose(f);
                printf("  Found test soundfont: %s\n", sfPath);
                // Test state restoration to load the soundfont
                std::vector<uint8_t> st(sizeof(float) * 44, 0);
                std::string extra = std::string(sfPath) + "|0";
                st.insert(st.end(), extra.begin(), extra.end());

                MockBStream strm;
                uint32_t version = 1000;
                uint32_t mode = 0;
                int32_t size = static_cast<int32_t>(st.size());
                strm.write(&version, sizeof(version), nullptr);
                strm.write(&mode, sizeof(mode), nullptr);
                strm.write(&size, sizeof(size), nullptr);
                strm.write(st.data(), static_cast<int32_t>(st.size()), nullptr);
                strm.cursor = 0;
                component->setState(&strm);

                printf("  Simulating MIDI playback across 4 blocks (2048 frames)...\n");
                float maxVal = 0.0f;
                for (int b = 0; b < 4; ++b) {
                    mockInEvents.events.clear();
                    if (b == 0) {
                        mockInEvents.addEvent(noteOnEv);
                    }
                    data.inputEvents = &mockInEvents;
                    processor->process(data);
                    for (int s = 0; s < 512; ++s) {
                        if (std::abs(outL[s]) > maxVal) maxVal = std::abs(outL[s]);
                        if (std::abs(outR[s]) > maxVal) maxVal = std::abs(outR[s]);
                    }
                }
                printf("  Peak audio output level: %.4f\n", maxVal);
            }
        }

        processor->setProcessing(false);
        processor->release();
    }

    // Query IEditController
    void* editObj = nullptr;
    res = component->queryInterface(IEditController_iid, &editObj);
    printf("queryInterface (IEditController) result: %d, obj=%p\n", res, editObj);

    if (res == kResultOk && editObj) {
        IEditController* controller = (IEditController*)editObj;
        int32 numParams = controller->getParameterCount();
        printf("Parameter count: %d\n", numParams);

        ParameterInfo pInfo;
        for (int i = 0; i < (numParams < 3 ? numParams : 3); ++i) {
            controller->getParameterInfo(i, pInfo);
            char title[128] = {0};
            utf16_to_char(pInfo.title, title);
            printf("  Param %d: '%s' (default=%.2f)\n", i, title, pInfo.defaultNormalizedValue);
        }

        // Test PlugView
        IPlugView* plugView = controller->createView(kEditor);
        printf("createView(kEditor): %p\n", plugView);
        if (plugView) {
            ViewRect rect;
            plugView->getSize(&rect);
            printf("Editor Rect: %dx%d (HWND supported: %s)\n",
                   rect.right - rect.left, rect.bottom - rect.top,
                   plugView->isPlatformTypeSupported(kPlatformTypeHWND) == kResultOk ? "YES" : "NO");
            plugView->release();
        }

        controller->release();
    }

    component->terminate();
    component->release();
    exitDll();
    FreeLibrary(hMod);

    printf("ALL CHECKS PASSED FOR %s!\n\n", dllPath);
    return 0;
}

