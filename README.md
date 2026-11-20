# ASSTER — mastering tool

**asster** is a plug-and-play **zero-clipping** **audio mastering plugin**.

---


## windows Hardware & DSP Optimizations

* **AVX2 & FMA SIMD Vectorization:** inner DSP loops compiled with `-O3 -mavx2 -mfma -ffast-math -fno-math-errno -fno-trapping-math` for Intel and AMD hardware acceleration.
* **block-Level metering optimization:** gain reduction computation in limiter and compressor moved out of per-sample loop into once-per-block scalar math, eliminating hundreds of thousands of redundant `log10` and `pow` calls per second.
* **exponential caching:** linear gain conversions use fast exponential lookup/conversions and conditional bypass when uncompressed.
* **zero audio thread allocation:** 64-bit double precision buffers pre-reserved to prevent thread memory reallocations during real-time rendering.
* **zero dependency static link:** statically linked C++ runtime with standard Windows OS libraries (`d2d1.dll`, `dwrite.dll`, `User32.dll`, `Kernel32.dll`, UCRT). runs on Windows x64 without requiring VC++ redistributables.

---

## building & installing

### requirements
* `g++` / MinGW-w64 (supporting C++17, AVX2, UCRT)
* `mingw32-make` or PowerShell / Batch

### quick build commands

```bash
# 1. run the comprehensive DSP test suite
mingw32-make test

# 2. compile the 64-bit DLL (dist/Asster.dll) via VeSTige
mingw32-make vestige

# 3. compile the VST3 64-bit plugin (dist/Asster.vst3)
mingw32-make vst3

# 4. compile standalone desktop GUI host (bin/asster_standalone.exe)
mingw32-make standalone

# 5. build distribution zip package
mingw32-make package

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
├── dist/
│   ├── Asster.dll            # 64-bit Windows plugin DLL (VeSTige)
│   ├── Asster.vst3           # 64-bit Windows VST3 plugin DLL (GPLv3)
│   └── Asster_Windows_x64.zip# distribution package
├── bin/
│   ├── asster_test.exe       # verification test suite 
│   ├── asster_standalone.exe # standalone desktop testing application
│   └── test_vst3_loader.exe  # VST3 test harness
├── include/
│   ├── vestige/vestige.h     # open-source VeSTige header (GPL)
│   ├── vst3.h                # single-header VST3 framework (GPLv3)
│   └── vst3_plugin.h         # VST3 plugin helper classes (GPLv3)
├── src/
│   ├── dsp/
│   │   ├── Biquad.hpp            # RBJ audio biquad filters
│   │   ├── LoudnessMeter.hpp     # ITU-R BS.1770 / EBU R128 & true Peak detector
│   │   ├── AutoTrim.hpp          # automatic loudness calibration engine
│   │   ├── LowEndMaster.hpp      # subsonic steep HPF & Linkwitz-Riley mono bass
│   │   ├── MasteringEQ.hpp       # 4-band mastering parametric sculptor
│   │   ├── HarmonicSaturator.hpp # analog Tape & Tube harmonic exciter
│   │   ├── StereoProcessor.hpp   # M/S width expander & side air sheen
│   │   ├── GlueCompressor.hpp    # SSL-style master bus VCA glue compressor
│   │   ├── SoftClipper.hpp       # cubic hermite soft-knee transient clipper
│   │   ├── TruePeakLimiter.hpp   # lookahead brickwall limiter
│   │   ├── AssterPresets.hpp     # presets
│   │   └── AssterDSP.hpp         # central mastering engine
│   ├── gui/
│   │   ├── Direct2DTheme.hpp     # Direct2D/DirectWrite theme system
│   │   └── WinMasteringUI.hpp    # Windows native Direct2D Monochrome Flat GUI
│   ├── vestige/
│   │   ├── AssterVestige.def     # clean export definitions
│   │   └── AssterVestige.cpp     # Windows VeSTige plugin wrapper
│   ├── vst3/
│   │   ├── AssterVST3.cpp        # native VST3 plugin implementation
│   │   ├── AssterVST3.def        # VST3 export definitions
│   │   ├── AssterVST3Editor.hpp  # VST3 IPlugView editor wrapper
│   │   └── test_loader.cpp       # VST3 host verification harness
│   └── standalone/
│       ├── main.cpp              # DSP test suite runner
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
- **`include/vst3.h`** & **`include/vst3_plugin.h`**: Open-source GPLv3 VST3 headers (derived from Steinberg VST3 SDK under GPLv3 and `vst3_plugin` by Tor-Helge Skei under GPLv3).
- *VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries.*
