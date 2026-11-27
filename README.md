# ASSTER — mastering tool

**asster** is a **zero-clipping** **audio mastering plugin** for Windows x64 (`VeSTige` DLL, native `VST3`, and standalone `Direct2D` host), featuring 54 parameters, 9 genre presets (Extreme Metal, Doom, Sludge, Groove, Modern Metalcore/Djent, Classic Heavy Metal, Transparent Master, Modern Pop, and Hip-Hop/Trap), an overall intensity control, a 3-second Auto-Master assistant, and custom Target LUFS (-24 to -3 LUFS).

---

## windows hardware, DSP & UI optimizations

* **SSE2, AVX2 & FMA SIMD vectorization:** inner DSP loops use SSE2 stereo-paired processing (`StereoBiquad` and vectorized `Oversampler`) and are compiled with `-O3 -mavx2 -mfma -fno-math-errno -fno-trapping-math -ffp-contract=fast -fno-signed-zeros -funroll-loops` for Intel and AMD hardware acceleration.
* **O(1) lookahead & oversampling-aware true-peak limiting:** `TruePeakLimiter` uses an O(1) circular monotonic deque and adapts inter-sample peak estimation to the active oversampling factor (4x Catmull-Rom at 1x OS, single midpoint tap at 2x OS, direct peak at 4x OS).
* **fast IEEE-754 math & sub-block updates:** linear/dB conversions use `FastMath.hpp` (`fastLog2`/`fastExp2`), while `DynamicEQ` and `SpectralStabilizer` update filter coefficients trig-free every 64 samples.
* **telemetry gating & zero audio-thread allocation:** `LoudnessMeter::setTelemetryEnabled` restricts FFT/scope/correlation ring writes to the output meter, and all DSP buffers are pre-allocated to prevent real-time thread reallocations.
* **lightweight Direct2D UI:** ~30 FPS refresh with zero-idle repaint gating (pauses `InvalidateRect` after 15 quiet frames when output is below -85.0 dBTP and no drag/calibration/auto-master is active), cached 96-point EQ curve, precomputed Radix-2 FFT twiddles, and allocation-free 22-segment polyline knob arcs.
* **zero-dependency static link:** statically linked C++ runtime with standard Windows OS libraries (`d2d1.dll`, `dwrite.dll`, `User32.dll`, `Kernel32.dll`, `comdlg32.dll`, `UCRT`). runs on Windows x64 without requiring VC++ redistributables.

### benchmark (5.0s stereo @ 44.1 kHz, all DSP stages active)
* **1x OS:** `31.47 ms` (`0.63%` real-time CPU)
* **2x OS:** `45.03 ms` (`0.90%` real-time CPU)
* **4x OS:** `52.79 ms` (`1.06%` real-time CPU)

---

## building & installing

### requirements
* `g++` / MinGW-w64 (supporting C++17, AVX2, UCRT)
* `mingw32-make` or PowerShell / Batch

### quick build commands

```bash
# 1. run the comprehensive DSP test suite & benchmark
mingw32-make test

# 2. compile the 64-bit DLL (dist/Asster.dll) via VeSTige & verify host load
mingw32-make vestige test_vst

# 3. compile the VST3 64-bit plugin (dist/Asster.vst3) & verify host load
mingw32-make vst3 test_vst3

# 4. compile standalone desktop GUI host (bin/asster_standalone.exe)
mingw32-make standalone

# 5. build all targets, run all tests, and create distribution zip package
mingw32-make all test_vst test_vst3 package

# 6. install into REAPER, LMMS, Common VST3, and VstPlugins directories
mingw32-make install
```

or run the build scripts:
```cmd
build.bat
```
or
```powershell
.\build.ps1
```

### installation paths (Windows)
* **Common VST3:** `%LOCALAPPDATA%\Programs\Common\VST3\Asster.vst3`
* **REAPER UserPlugins:** `%APPDATA%\REAPER\UserPlugins\Asster.dll`
* **VstPlugins:** `%USERPROFILE%\Documents\VstPlugins\Asster.dll` & `Asster.vst3`
* **LMMS VST:** `%USERPROFILE%\Documents\lmms\plugins\vst\Asster.dll`

---

## project architecture

```
mastering plugin/
├── include/
│   ├── vestige/vestige.h     # open-source VeSTige header (GPL)
│   ├── vst3.h                # single-header VST3 framework (GPLv3)
│   └── vst3_plugin.h         # VST3 plugin helper classes (GPLv3)
├── src/
│   ├── dsp/
│   │   ├── Biquad.hpp            # RBJ audio biquad & SSE2 StereoBiquad filters
│   │   ├── FastMath.hpp          # fast IEEE-754 log2/exp2 DSP math approximations
│   │   ├── LoudnessMeter.hpp     # ITU-R BS.1770 / EBU R128 & True Peak detector
│   │   ├── AutoTrim.hpp          # automatic loudness calibration engine
│   │   ├── LowEndMaster.hpp      # subsonic steep HPF & Linkwitz-Riley mono bass
│   │   ├── MasteringEQ.hpp       # 4-band mastering parametric & M/S sculptor
│   │   ├── DynamicEQ.hpp         # 2-band dynamic EQ (Chug Control & De-Harsh)
│   │   ├── SpectralStabilizer.hpp# 5-band continuous adaptive spectral stabilizer
│   │   ├── HarmonicSaturator.hpp # analog Tape, Tube & Solid harmonic exciter
│   │   ├── StereoProcessor.hpp   # M/S width expander & side air sheen
│   │   ├── GlueCompressor.hpp    # SSL-style bus VCA glue (SC HPF + parallel mix)
│   │   ├── MultibandComp.hpp     # 3-band LR4 mastering multiband compressor
│   │   ├── TransientShaper.hpp   # dual-envelope attack punch & sustain shaper
│   │   ├── AutoMaster.hpp        # 3-second spectral & loudness assistant
│   │   ├── SoftClipper.hpp       # cubic hermite soft-knee transient clipper
│   │   ├── TruePeakLimiter.hpp   # O(1) deque lookahead true-peak limiter (Clean/Punch/Crush)
│   │   ├── Oversampler.hpp       # SSE2 8th-order polyphase half-band IIR (1x/2x/4x)
│   │   ├── Dither.hpp            # TPDF dithering with 1st-order noise shaping
│   │   ├── AssterPresets.hpp     # 9 genre presets (metal, transparent, pop, hip-hop)
│   │   └── AssterDSP.hpp         # lock-free mastering engine, A/B & state persistence
│   ├── gui/
│   │   ├── Direct2DTheme.hpp     # Direct2D/DirectWrite theme system
│   │   ├── SpectrumFFT.hpp       # 2048-point Radix-2 FFT spectrum analyzer
│   │   ├── ReferenceAnalyzer.hpp # WAV reference track spectral overlay analyzer
│   │   └── WinMasteringUI.hpp    # 1120x720 Windows native Direct2D GUI
│   ├── vestige/
│   │   ├── AssterVestige.def     # clean export definitions
│   │   └── AssterVestige.cpp     # Windows VeSTige plugin wrapper
│   ├── vst3/
│   │   ├── AssterVST3.cpp        # native VST3 plugin implementation
│   │   ├── AssterVST3.def        # VST3 export definitions
│   │   ├── AssterVST3Editor.hpp  # VST3 IPlugView editor wrapper
│   │   └── test_loader.cpp       # VST3 host verification harness
│   └── standalone/
│       ├── main.cpp              # DSP test suite & benchmark runner
│       └── gui_main.cpp          # Windows native standalone host runner
├── Makefile                      # Windows x64 Makefile
├── build.bat                     # Windows cmd build script
├── build.ps1                     # Windows PowerShell build script
├── test_vestige.ps1              # VeSTige host verification script
└── README.md
```

---

## license & third-party notices

this project is licensed under the **GNU General Public License v3.0 (GPL-3.0)** — see [LICENSE](LICENSE).

- **`include/vestige/vestige.h`**: VeSTige compatibility header (GNU GPL v2 or later, originally by Javier Serrano Polo; sourced from Carla/LMMS).
- **`include/vst3.h`** & **`include/vst3_plugin.h`**: open-source GPLv3 VST3 headers (derived from Steinberg VST3 SDK under GPLv3 and `vst3_plugin` by Tor-Helge Skei under GPLv3).
- *VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries.*
