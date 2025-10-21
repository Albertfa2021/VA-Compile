# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is a **build documentation repository** for Virtual Acoustics (VA) v2022a, a complex audio rendering framework developed by RWTH Aachen University. The actual source code is located at `D:\Project\VA2022a\VA_source_code`.

**Important**: VA v2022a is being compiled, NOT the latest version. The correct tag is:
https://git.rwth-aachen.de/ita/VA/-/tree/VA_v2022a?ref_type=tags

## Build System Architecture

VA uses a **multi-repository structure** with heavy use of Git submodules:
- **Build System**: CMake with Visual Studio 2022 generator
- **Dependencies Manager**: VistaCMakeCommon (required CMake extension from ViSTA VR Toolkit)
- **Platform**: Windows 10/11 x64 with MSVC

### Key Components
- **VACore**: Core audio rendering library (DLL)
- **VAServer**: Standalone server application
- **VAMatlab**: Matlab R2024a bindings (MEX interface)
- **Renderers**: Multiple audio renderers including BinauralOutdoorNoise (the primary focus)

## CMake Configuration

### Initial Configuration Command
```bash
cd D:\Project\VA2022a\VA_source_code\build

cmake .. -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE="D:/Project/VA2022a/va_toolchain.cmake" \
  -DNatNetSDK_DIR="D:/Project/VA2022a/NatNetSDK" \
  -DITA_VA_WITH_CORE=ON \
  -DITA_VA_WITH_SERVER_APP=ON \
  -DITA_VA_WITH_BINDING_MATLAB=ON \
  -DITA_VA_WITH_BINDING_PYTHON=OFF
```

**Critical**: `NatNetSDK_DIR` MUST be passed on the command line, not just in toolchain file, due to CMake variable scoping issues.

### Build Commands
```bash
# Build in Release mode with parallel compilation
cmake --build . --config Release -j8

# Build and install (generates distribution package)
cmake --build . --config Release --target INSTALL -j8
```

### Clean Build
```bash
# Full cache clean (required when switching configurations)
cd D:\Project\VA2022a\VA_source_code\build
rm -rf CMakeCache.txt CMakeFiles _deps/vista-subbuild
```

## Known Issues and Fixes Applied

The codebase required **11 major fixes** to compile on Windows with VS2022. These are documented in `VA_COMPILATION_PROCESS.md`:

### Critical API Changes
**ITACoreLibs EVDLAlgorithm Migration**: The Variable Delay Line API changed from class member enums to enum class:
- Old: `CITAVariableDelayLine::SWITCH`
- New: `EVDLAlgorithm::SWITCH`
- **Fix Applied**: All variable declarations changed from `int` to `EVDLAlgorithm` type

### Platform-Specific Fixes
1. **NOMINMAX**: Added to VACore CMakeLists.txt to prevent Windows.h min/max macro conflicts
2. **FFTW libm**: Modified to skip linking libm on MSVC (line 326-328 in fftw CMakeLists.txt)
3. **Character Encoding**: Replaced degree symbol (°) with "degrees" in VADirectivityDAFFEnergetic.cpp:83

### Dependency Issues
1. **NatNetSDK**: FindNatNetSDK.cmake must be in VistaCMakeCommon directory
2. **protobuf_generate**: Manually download if `file(DOWNLOAD)` fails (creates 0-byte file)
3. **SketchUpAPI**: Added `SketchUpAPI_FOUND` check before setting properties

## File Modifications Summary

### Modified VACore Files (API compatibility)
Files with `CITAVariableDelayLine` → `EVDLAlgorithm` changes:
- `src/Rendering/Base/VAAudioRendererBase.{cpp,h}`
- `src/Rendering/Base/VAFIRRendererBase.cpp`
- `src/Rendering/Base/VASoundPathRendererBase.cpp`
- All renderer files in `src/Rendering/Ambisonics/`, `Binaural/`, `Prototyping/`

### Modified Dependency Files
- `build/_deps/ihtautilitylibs-src/IHTATracking/external_libs/CMakeLists.txt` (line 52: NatNetSDK detection)
- `build/_deps/itageometricalacoustics-src/ITAGeo/CMakeLists.txt` (line 43: SketchUpAPI check)
- `build/_deps/fftw-src/CMakeLists.txt` (line 326: libm linking)
- `VACore/src/DataHistory/VADataHistoryModel_impl.h` (line 109: macro expansion fix)

## Matlab Integration

### Installation Location
Distribution generated at: `D:\Project\VA2022a\VA_source_code\build\dist\VAMatlab_v2022a\`

### Matlab Setup
```matlab
% Add to Matlab path
addpath('D:\Project\VA2022a\VA_source_code\build\dist\VAMatlab_v2022a')
setenv('PATH', [getenv('PATH') ';D:\Project\VA2022a\VA_source_code\build\dist\bin'])

% Test installation
va = VA();
```

### Available Examples
11 example scripts included, key ones:
- `VA_example_outdoor_acoustics.m` - BinauralOutdoorNoise renderer
- `VA_example_simple.m` - Basic usage
- `VA_example_offline_simulation_outdoors.m` - Batch processing

## Important Paths

```
D:\Project\VA2022a\
├── VA_source_code\           # Main source repository
│   └── build\                # CMake build directory
│       └── dist\             # Installation output
├── VistaCMakeCommon\         # CMake modules (FindNatNetSDK.cmake)
├── NatNetSDK\                # Motion capture SDK
└── va_toolchain.cmake        # Toolchain configuration
```

## Working with Submodules

VA is a collective of repositories. When updating:
```bash
# Update all submodules to latest
git submodule update --remote

# Checkout branch in all submodules
git submodule foreach git checkout master

# Switch to specific version
cd VACore && git checkout v2022.a
```

## Build Artifacts

**Success Indicators**:
- `build/Release/bin/VACore.dll` (~1.8 MB)
- `build/Release/bin/VAServer.exe`
- `build/dist/VAMatlab_v2022a/@VA/private/VAMatlab.mexw64` (~15-20 MB)

## Compilation Notes

- **First build**: Takes 15-30 minutes (downloads dependencies: sndfile, eigen, assimp, HDF5, gRPC)
- **Incremental builds**: 2-3 minutes
- **Expected warnings**: ~50 warnings (mostly encoding and type conversion, non-critical)
- **Parallel jobs**: Use `-j8` for faster compilation

## Critical Build Requirements

1. Visual Studio 2022 with C++ Desktop Development workload
2. Matlab R2024a (for Matlab bindings)
3. VistaCMakeCommon in CMAKE_MODULE_PATH
4. All dependency paths must use forward slashes in CMake (e.g., `D:/Project/...`)
