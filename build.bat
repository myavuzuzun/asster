@echo off
setlocal
echo ========================================================
echo   ASSTER Mastering Tool - Windows x64 Build System
echo ========================================================

where g++ >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: g++ was not found in PATH.
    exit /b 1
)

if not exist bin mkdir bin
if not exist dist mkdir dist

echo [1/3] Building & Running DSP Test Suite...
g++ -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Iinclude -fno-math-errno -fno-trapping-math src/standalone/main.cpp -o bin/asster_test.exe
if %ERRORLEVEL% neq 0 (
    echo DSP Test compilation failed!
    exit /b 1
)
bin\asster_test.exe
if %ERRORLEVEL% neq 0 (
    echo DSP Tests failed!
    exit /b 1
)

echo [2/4] Building VeSTige Plugin DLL (dist/Asster.dll)...
g++ -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Iinclude -fno-math-errno -fno-trapping-math -shared -static src/vestige/AssterVestige.cpp src/vestige/AssterVestige.def -ld2d1 -ldwrite -lole32 -lgdi32 -luser32 -o dist/Asster.dll
if %ERRORLEVEL% neq 0 (
    echo VeSTige DLL compilation failed!
    exit /b 1
)
echo Successfully built dist\Asster.dll

echo [3/4] Building VST3 Plugin DLL (dist/Asster.vst3)...
g++ -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-private-field -Iinclude -fno-math-errno -fno-trapping-math -shared -static src/vst3/AssterVST3.cpp src/vst3/AssterVST3.def -ld2d1 -ldwrite -lole32 -lgdi32 -luser32 -o dist/Asster.vst3
if %ERRORLEVEL% neq 0 (
    echo VST3 compilation failed!
    exit /b 1
)
echo Successfully built dist\Asster.vst3

echo [4/4] Building Standalone Host (bin/asster_standalone.exe)...
g++ -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Iinclude -fno-math-errno -fno-trapping-math -static src/standalone/gui_main.cpp -ld2d1 -ldwrite -lole32 -lgdi32 -luser32 -o bin/asster_standalone.exe
if %ERRORLEVEL% neq 0 (
    echo Standalone host compilation failed!
    exit /b 1
)
echo Successfully built bin\asster_standalone.exe

echo.
echo ========================================================
echo   ALL TARGETS BUILT SUCCESSFULLY!
echo ========================================================

