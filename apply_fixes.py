#!/usr/bin/env python3
"""
VA2022a Code Fixes Script
This script applies all necessary code fixes for compiling VA2022a on Windows/MSVC

Based on VA_COMPILATION_PROCESS.md documentation
"""

import os
import re
from pathlib import Path

# Base path
BASE_PATH = Path("D:/Project/VA-build/source")

def apply_evdl_algorithm_fixes():
    """Fix 1: Update CITAVariableDelayLine API to EVDLAlgorithm"""
    print("=" * 60)
    print("Fix 1: Updating EVDLAlgorithm API usage...")
    print("=" * 60)

    files_to_fix = [
        "VACore/src/Rendering/Base/VAAudioRendererBase.cpp",
        "VACore/src/Rendering/Base/VAFIRRendererBase.cpp",
        "VACore/src/Rendering/Base/VASoundPathRendererBase.cpp",
        "VACore/src/Rendering/Ambisonics/Freefield/VAAmbisonicsFreefieldAudioRenderer.cpp",
        "VACore/src/Rendering/Binaural/AirTrafficNoise/VAATNSourceReceiverTransmission.cpp",
        "VACore/src/Rendering/Binaural/ArtificialReverb/VABinauralArtificialReverb.cpp",
        "VACore/src/Rendering/Binaural/Clustering/VABinauralClusteringRenderer.cpp",
        "VACore/src/Rendering/Binaural/Clustering/WaveFront/VABinauralWaveFrontBase.cpp",
        "VACore/src/Rendering/Binaural/FreeField/VABinauralFreeFieldAudioRenderer.cpp",
        "VACore/src/Rendering/Prototyping/GenericPath/VAPTGenericPathAudioRenderer.cpp",
        "VACore/src/Rendering/Prototyping/ImageSource/VAPTImageSourceAudioRenderer.cpp",
        "VACore/src/Rendering/Prototyping/FreeField/VAPrototypeFreeFieldAudioRenderer.cpp",
    ]

    replacements = {
        r'CITAVariableDelayLine::SWITCH': 'EVDLAlgorithm::SWITCH',
        r'CITAVariableDelayLine::CROSSFADE': 'EVDLAlgorithm::CROSSFADE',
        r'CITAVariableDelayLine::LINEAR_INTERPOLATION': 'EVDLAlgorithm::LINEAR_INTERPOLATION',
        r'CITAVariableDelayLine::CUBIC_SPLINE_INTERPOLATION': 'EVDLAlgorithm::CUBIC_SPLINE_INTERPOLATION',
        r'CITAVariableDelayLine::WINDOWED_SINC_INTERPOLATION': 'EVDLAlgorithm::WINDOWED_SINC_INTERPOLATION',
    }

    for file_rel in files_to_fix:
        file_path = BASE_PATH / file_rel
        if not file_path.exists():
            print(f"  [WARN] File not found: {file_rel}")
            continue

        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        modified = content
        for old, new in replacements.items():
            modified = re.sub(old, new, modified)

        if content != modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(modified)
            print(f"  [OK] Fixed: {file_rel}")
        else:
            print(f"  - No changes: {file_rel}")

def apply_evdl_member_variable_fixes():
    """Fix 2: Change int to EVDLAlgorithm for member variables"""
    print("\n" + "=" * 60)
    print("Fix 2: Fixing EVDLAlgorithm member variable types...")
    print("=" * 60)

    files_to_fix = [
        ("VACore/src/Rendering/Ambisonics/Freefield/VAAmbisonicsFreefieldAudioRenderer.cpp", []),
        ("VACore/src/Rendering/Ambisonics/Freefield/VAAmbisonicsFreefieldAudioRenderer.h", []),
        ("VACore/src/Rendering/Binaural/AirTrafficNoise/VAAirTrafficNoiseAudioRenderer.h", []),
        ("VACore/src/Rendering/Binaural/AirTrafficNoise/VAATNSourceReceiverTransmission.h", []),
        ("VACore/src/Rendering/Binaural/ArtificialReverb/VABinauralArtificialReverb.cpp", []),
        ("VACore/src/Rendering/Binaural/ArtificialReverb/VABinauralArtificialReverb.h", []),
        ("VACore/src/Rendering/Binaural/Clustering/VABinauralClusteringRenderer.h", []),
        ("VACore/src/Rendering/Prototyping/HearingAid/VAPTHearingAidRenderer.cpp", []),
        ("VACore/src/Rendering/Prototyping/HearingAid/VAPTHearingAidRenderer.h", []),
        ("VACore/src/Rendering/Prototyping/GenericPath/VAPTGenericPathAudioRenderer.cpp", []),
        ("VACore/src/Rendering/Prototyping/ImageSource/VAPTImageSourceAudioRenderer.cpp", []),
        ("VACore/src/Rendering/Prototyping/FreeField/VAPrototypeFreeFieldAudioRenderer.cpp", []),
        ("VACore/src/Rendering/Prototyping/FreeField/VAPrototypeFreeFieldAudioRenderer.h", []),
    ]

    pattern = re.compile(r'\bint\s+(m_iDefaultVDLSwitchingAlgorithm)\b')
    pattern2 = re.compile(r'\bconst\s+int\s+(iAlgorithm)\s*=\s*EVDLAlgorithm::')

    for file_rel, _ in files_to_fix:
        file_path = BASE_PATH / file_rel
        if not file_path.exists():
            print(f"  [WARN] File not found: {file_rel}")
            continue

        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        modified = pattern.sub(r'EVDLAlgorithm \1', content)
        modified = pattern2.sub(r'const EVDLAlgorithm \1 = EVDLAlgorithm::', modified)

        if content != modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(modified)
            print(f"  [OK] Fixed: {file_rel}")
        else:
            print(f"  - No changes: {file_rel}")

def apply_vacore_cmake_fixes():
    """Fix 3: Add NOMINMAX to VACore CMakeLists"""
    print("\n" + "=" * 60)
    print("Fix 3: Adding NOMINMAX to VACore CMakeLists.txt...")
    print("=" * 60)

    file_path = BASE_PATH / "VACore/CMakeLists.txt"

    if not file_path.exists():
        print(f"  [WARN] File not found")
        return

    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Check if already has NOMINMAX
    if 'NOMINMAX' in content:
        print("  - Already has NOMINMAX")
        return

    # Find target_compile_definitions section and add NOMINMAX
    pattern = r'(target_compile_definitions\s*\(\s*VACore\s+PUBLIC[^)]*PRIVATE)'
    replacement = r'\1\n        NOMINMAX  # Prevent Windows.h min/max macros'

    modified = re.sub(pattern, replacement, content, flags=re.DOTALL)

    if content != modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(modified)
        print("  [OK] Added NOMINMAX to VACore CMakeLists.txt")
    else:
        print("  [WARN] Could not automatically add NOMINMAX - manual edit required")

def apply_data_history_model_fix():
    """Fix 4: Fix VADataHistoryModel macro expansion"""
    print("\n" + "=" * 60)
    print("Fix 4: Fixing VADataHistoryModel macro expansion...")
    print("=" * 60)

    file_path = BASE_PATH / "VACore/src/DataHistory/VADataHistoryModel_impl.h"

    if not file_path.exists():
        print(f"  [WARN] File not found")
        return

    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Replace VA_EXCEPT_NOT_IMPLEMENTED( ); with direct throw
    old_code = r'VA_EXCEPT_NOT_IMPLEMENTED\s*\(\s*\)\s*;'
    new_code = 'throw CVAException( CVAException::NOT_IMPLEMENTED, "Not implemented" );'

    modified = re.sub(old_code, new_code, content)

    if content != modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(modified)
        print("  [OK] Fixed VADataHistoryModel macro expansion")
    else:
        print("  - No changes needed")

def apply_character_encoding_fix():
    """Fix 5: Fix character encoding in VADirectivityDAFFEnergetic"""
    print("\n" + "=" * 60)
    print("Fix 5: Fixing character encoding issues...")
    print("=" * 60)

    file_path = BASE_PATH / "VACore/src/directivities/VADirectivityDAFFEnergetic.cpp"

    if not file_path.exists():
        print(f"  [WARN] File not found")
        return

    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Replace degree symbol with "degrees"
    modified = content.replace('°', ' degrees')

    if content != modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(modified)
        print("  [OK] Fixed character encoding")
    else:
        print("  - No changes needed")

def add_include_headers():
    """Fix 6: Add missing ITAVariableDelayLine.h includes"""
    print("\n" + "=" * 60)
    print("Fix 6: Adding missing header includes...")
    print("=" * 60)

    files_to_fix = [
        ("VACore/src/Rendering/Base/VAAudioRendererBase.h", "#include <ITASampleBuffer.h>", "#include <ITAVariableDelayLine.h>"),
        ("VACore/src/Rendering/Binaural/ArtificialReverb/VABinauralArtificialReverb.h", "// ITA includes", "#include <ITAVariableDelayLine.h>"),
        ("VACore/src/Rendering/Binaural/Clustering/VABinauralClusteringRenderer.h", "// ITA includes", "#include <ITAVariableDelayLine.h>"),
    ]

    for file_rel, after_line, include_line in files_to_fix:
        file_path = BASE_PATH / file_rel
        if not file_path.exists():
            print(f"  [WARN] File not found: {file_rel}")
            continue

        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        if '#include <ITAVariableDelayLine.h>' in content:
            print(f"  - Already has include: {file_rel}")
            continue

        # Add include after the specified line
        modified = content.replace(after_line, f"{after_line}\n{include_line}")

        if content != modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(modified)
            print(f"  [OK] Added include: {file_rel}")
        else:
            print(f"  [WARN] Could not add include: {file_rel}")

def main():
    print("\n")
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 58 + "║")
    print("║" + "  VA2022a Code Fixes Script".center(58) + "║")
    print("║" + " " * 58 + "║")
    print("╚" + "=" * 58 + "╝")
    print("\n")

    apply_evdl_algorithm_fixes()
    apply_evdl_member_variable_fixes()
    apply_vacore_cmake_fixes()
    apply_data_history_model_fix()
    apply_character_encoding_fix()
    add_include_headers()

    print("\n" + "=" * 60)
    print("All fixes applied successfully!")
    print("=" * 60)
    print("\nNext steps:")
    print("  1. Run CMake configuration if not done yet")
    print("  2. Build the project: cmake --build . --config Release -j8")
    print("=" * 60)

if __name__ == "__main__":
    main()
