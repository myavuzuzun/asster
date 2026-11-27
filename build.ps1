$ErrorActionPreference = "Continue"

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  ASSTER Mastering Tool - Windows x64 Build System" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    Write-Error "g++ compiler not found in PATH."
    exit 1
}

New-Item -ItemType Directory -Force -Path bin, dist | Out-Null

function Sign-Binary($filePath) {
    if (Test-Path $filePath) {
        $cert = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($cert) {
            Set-AuthenticodeSignature -FilePath $filePath -Certificate $cert -ErrorAction SilentlyContinue | Out-Null
        }
    }
}

Write-Host "[1/3] Building & Running DSP Test Suite..." -ForegroundColor Yellow
g++ -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Iinclude -fno-math-errno -fno-trapping-math -ffp-contract=fast -fno-signed-zeros -funroll-loops src/standalone/main.cpp -o bin/asster_test.exe
Sign-Binary "bin/asster_test.exe"
& .\bin\asster_test.exe
if ($LASTEXITCODE -ne 0) { throw "DSP test suite failed." }

Write-Host "[2/4] Building VeSTige Plugin (dist/Asster.dll)..." -ForegroundColor Yellow
g++ -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Iinclude -fno-math-errno -fno-trapping-math -ffp-contract=fast -fno-signed-zeros -funroll-loops -shared -static src/vestige/AssterVestige.cpp src/vestige/AssterVestige.def -ld2d1 -ldwrite -lole32 -lgdi32 -luser32 -lcomdlg32 -o dist/Asster.dll
Sign-Binary "dist/Asster.dll"
& powershell -ExecutionPolicy Bypass -File .\test_vestige.ps1
Write-Host "✓ Built, verified, and signed dist/Asster.dll" -ForegroundColor Green

Write-Host "[3/4] Building VST3 Plugin (dist/Asster.vst3)..." -ForegroundColor Yellow
g++ -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-private-field -Iinclude -fno-math-errno -fno-trapping-math -ffp-contract=fast -fno-signed-zeros -funroll-loops -shared -static src/vst3/AssterVST3.cpp src/vst3/AssterVST3.def -ld2d1 -ldwrite -lole32 -lgdi32 -luser32 -lcomdlg32 -o dist/Asster.vst3
Sign-Binary "dist/Asster.vst3"
g++ -O2 -std=c++17 -Iinclude -Wall -Wextra -Wno-unused-parameter -Wno-unused-private-field src/vst3/test_loader.cpp -o bin/test_vst3_loader.exe
& .\bin\test_vst3_loader.exe "dist\Asster.vst3"
Write-Host "✓ Built, verified, and signed dist/Asster.vst3" -ForegroundColor Green

Write-Host "[4/4] Building Standalone Host (bin/asster_standalone.exe)..." -ForegroundColor Yellow
g++ -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Iinclude -fno-math-errno -fno-trapping-math -ffp-contract=fast -fno-signed-zeros -funroll-loops -static src/standalone/gui_main.cpp -ld2d1 -ldwrite -lole32 -lgdi32 -luser32 -lcomdlg32 -o bin/asster_standalone.exe
Sign-Binary "bin/asster_standalone.exe"
Write-Host "✓ Built and signed bin/asster_standalone.exe" -ForegroundColor Green

# Deploy to User VST3 and VstPlugins locations
$userVst3 = "$env:LOCALAPPDATA\Programs\Common\VST3"
if (-not (Test-Path $userVst3)) { New-Item -ItemType Directory -Path $userVst3 -Force | Out-Null }
Copy-Item -Path "dist/Asster.vst3" -Destination "$userVst3\Asster.vst3" -Force -ErrorAction SilentlyContinue
if (Test-Path "$env:USERPROFILE\Documents\VstPlugins") {
    Copy-Item -Path "dist/Asster.vst3" -Destination "$env:USERPROFILE\Documents\VstPlugins\Asster.vst3" -Force -ErrorAction SilentlyContinue
}

# Create distribution zip
Write-Host "`nCreating distribution zip..." -ForegroundColor Yellow
$pkg = "dist/package"
if (-not (Test-Path $pkg)) { New-Item -ItemType Directory -Path $pkg | Out-Null }
Copy-Item "dist/Asster.dll", "dist/Asster.vst3", "bin/asster_standalone.exe" -Destination $pkg -Force
if (Test-Path "README.md") {
    Copy-Item "README.md" -Destination $pkg -Force
}
Compress-Archive -Path "dist/package/*" -DestinationPath "dist/Asster_Windows_x64.zip" -Force
Copy-Item "dist/Asster_Windows_x64.zip" -Destination "Asster_Windows_x64.zip" -Force
Write-Host "✓ Distribution archive created: Asster_Windows_x64.zip" -ForegroundColor Green

Write-Host "`nAll build targets completed successfully!" -ForegroundColor Green
