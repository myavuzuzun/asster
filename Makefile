CXX ?= g++

# -------------------------------------------------------------
# Windows 64-bit Build Configuration (Direct2D / DirectWrite)
# -------------------------------------------------------------
CXXFLAGS ?= -O3 -mavx2 -mfma -std=c++17 -Wall -Wextra -Iinclude -fno-math-errno -fno-trapping-math -static
UI_LIBS = -ld2d1 -ldwrite -lole32 -lgdi32 -luser32

DIST_DIR = dist
BIN_DIR = bin

TARGET_DLL = $(DIST_DIR)/Asster.dll
TARGET_DLL_WIN = $(DIST_DIR)\Asster.dll
TARGET_VST3 = $(DIST_DIR)/Asster.vst3
TARGET_VST3_WIN = $(DIST_DIR)\Asster.vst3
TEST_EXE = $(BIN_DIR)/asster_test.exe
TEST_VST3_EXE = $(BIN_DIR)/test_vst3_loader.exe
STANDALONE_EXE = $(BIN_DIR)/asster_standalone.exe

REAPER_PATH = $(APPDATA)\REAPER\UserPlugins
LMMS_PATH = $(USERPROFILE)\Documents\lmms\plugins\vst
VST_PATH = $(USERPROFILE)\Documents\VstPlugins
USER_VST3_PATH = $(LOCALAPPDATA)\Programs\Common\VST3

all: test vestige vst3 standalone

test: $(TEST_EXE)
	@echo Running Asster DSP Test Suite...
	@$(TEST_EXE)

$(TEST_EXE): src/standalone/main.cpp src/dsp/*.hpp
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	$(CXX) $(CXXFLAGS) src/standalone/main.cpp -o $(TEST_EXE)

test_vst: vestige
	@echo Running VeSTige Host Verification...
	@powershell -ExecutionPolicy Bypass -File .\test_vestige.ps1

test_vst3: $(TEST_VST3_EXE) vst3
	@echo Running VST3 Host Verification...
	@$(TEST_VST3_EXE) $(TARGET_VST3_WIN)

$(TEST_VST3_EXE): src/vst3/test_loader.cpp
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	$(CXX) -O2 -std=c++17 -Iinclude -Wall -Wextra -Wno-unused-parameter -Wno-unused-private-field src/vst3/test_loader.cpp -o $(TEST_VST3_EXE)

vestige: $(TARGET_DLL)

$(TARGET_DLL): src/vestige/AssterVestige.cpp src/vestige/AssterVestige.def src/dsp/*.hpp src/gui/WinMasteringUI.hpp include/vestige/vestige.h
	@if not exist $(DIST_DIR) mkdir $(DIST_DIR)
	$(CXX) $(CXXFLAGS) -shared src/vestige/AssterVestige.cpp src/vestige/AssterVestige.def $(UI_LIBS) -o $(TARGET_DLL)
	@echo Build successful: $(TARGET_DLL)

vst3: $(TARGET_VST3)

$(TARGET_VST3): src/vst3/AssterVST3.cpp src/vst3/AssterVST3.def src/dsp/*.hpp src/gui/WinMasteringUI.hpp src/vst3/AssterVST3Editor.hpp
	@if not exist $(DIST_DIR) mkdir $(DIST_DIR)
	$(CXX) $(CXXFLAGS) -Wno-unused-parameter -Wno-unused-private-field -shared src/vst3/AssterVST3.cpp src/vst3/AssterVST3.def $(UI_LIBS) -o $(TARGET_VST3)
	@echo VST3 build successful: $(TARGET_VST3)

standalone: $(STANDALONE_EXE)

$(STANDALONE_EXE): src/standalone/gui_main.cpp src/dsp/*.hpp src/gui/WinMasteringUI.hpp
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)
	$(CXX) $(CXXFLAGS) src/standalone/gui_main.cpp $(UI_LIBS) -o $(STANDALONE_EXE)
	@echo Standalone host built: $(STANDALONE_EXE)

install: vestige vst3
	@echo Installing Asster plugins...
	@if exist "$(REAPER_PATH)" copy /Y $(TARGET_DLL_WIN) "$(REAPER_PATH)\"
	@if exist "$(LMMS_PATH)" copy /Y $(TARGET_DLL_WIN) "$(LMMS_PATH)\"
	@if exist "$(VST_PATH)" copy /Y $(TARGET_DLL_WIN) "$(VST_PATH)\"
	@if exist "$(VST_PATH)" copy /Y $(TARGET_VST3_WIN) "$(VST_PATH)\"
	@if not exist "$(USER_VST3_PATH)" mkdir "$(USER_VST3_PATH)"
	@copy /Y $(TARGET_VST3_WIN) "$(USER_VST3_PATH)\"
	@echo Installation complete!

package: vestige vst3 standalone
	@powershell -ExecutionPolicy Bypass -Command "$$p='dist/package'; if (Test-Path $$p) { Remove-Item -Recurse -Force $$p }; New-Item -ItemType Directory -Path $$p | Out-Null; Copy-Item 'dist/Asster.dll', 'dist/Asster.vst3', 'bin/asster_standalone.exe' -Destination $$p; Copy-Item 'package_README.txt' -Destination \"$$p/README.txt\" -Force; Compress-Archive -Path 'dist/package/*' -DestinationPath 'dist/Asster_Windows_x64.zip' -Force; Copy-Item 'dist/Asster_Windows_x64.zip' -Destination 'Asster_Windows_x64.zip' -Force"
	@echo Package created: dist/Asster_Windows_x64.zip and Asster_Windows_x64.zip

clean:
	@if exist $(DIST_DIR) rmdir /S /Q $(DIST_DIR)
	@if exist $(BIN_DIR) rmdir /S /Q $(BIN_DIR)

.PHONY: all test test_vst test_vst3 vestige vst3 standalone install package clean
