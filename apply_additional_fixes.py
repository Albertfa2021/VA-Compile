#!/usr/bin/env python3
"""
VA2022a Additional Code Fixes Script
Fixes discovered during compilation:
1. Remove UNICODE definitions (causes ASIO/TBB compilation errors)
2. Update NatNetSDK FindModule to use static library
3. Fix FFTW m.lib dependency issue
"""

import os
import re
from pathlib import Path

# Base path
BASE_PATH = Path("D:/Project/VA-build")

def fix_fftw_cmake():
    """Fix FFTW m.lib issue by removing math library dependency"""
    print("=" * 60)
    print("Fix: Removing m.lib dependency from FFTW...")
    print("=" * 60)

    file_path = BASE_PATH / "build/_deps/fftw-src/CMakeLists.txt"

    if not file_path.exists():
        print(f"  [WARN] File not found: {file_path}")
        return

    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Remove m library dependency (Unix only, not needed on Windows)
    modified = re.sub(r'\$\{MATHLIB\}', '', content)
    modified = re.sub(r'list\s*\(\s*APPEND\s+fftw3f_lib\s+\$\{MATHLIB\}\s*\)', '', modified)

    if content != modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(modified)
        print(f"  [OK] Fixed FFTW CMakeLists.txt")
    else:
        print(f"  - No changes needed")

def update_natnetsdk_findmodule():
    """Update NatNetSDK FindModule to use static library"""
    print("\n" + "=" * 60)
    print("Fix: Updating NatNetSDK FindModule to use static library...")
    print("=" * 60)

    files_to_fix = [
        BASE_PATH / "dependencies/VistaCMakeCommon/FindNatNetSDK.cmake",
        BASE_PATH / "build/_deps/ihtautilitylibs-src/IHTATracking/external_libs/FindNatNetSDK.cmake"
    ]

    for file_path in files_to_fix:
        if not file_path.exists():
            print(f"  [WARN] File not found: {file_path}")
            continue

        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # Change to use NatNetLibStatic.lib for better compatibility
        old_code = '''    find_library(NatNetSDK_LIBRARY
        NAMES NatNet NatNetLib
        PATHS
            ${NatNetSDK_ROOT_DIR}/lib
            ${NatNetSDK_ROOT_DIR}/lib/${NATNET_LIB_SUFFIX}
            ${NatNetSDK_DIR}/lib
            ${NatNetSDK_DIR}/lib/${NATNET_LIB_SUFFIX}
        NO_DEFAULT_PATH
    )'''

        new_code = '''    find_library(NatNetSDK_LIBRARY
        NAMES NatNetLibStatic NatNetLib NatNet
        PATHS
            ${NatNetSDK_ROOT_DIR}/lib/${NATNET_LIB_SUFFIX}
            ${NatNetSDK_ROOT_DIR}/lib
            ${NatNetSDK_DIR}/lib/${NATNET_LIB_SUFFIX}
            ${NatNetSDK_DIR}/lib
        NO_DEFAULT_PATH
    )'''

        modified = content.replace(old_code, new_code)

        if content != modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(modified)
            print(f"  [OK] Updated: {file_path.name}")
        else:
            print(f"  - No changes: {file_path.name}")

def main():
    print("\n")
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 58 + "║")
    print("║" + "  VA2022a Additional Fixes Script".center(58) + "║")
    print("║" + " " * 58 + "║")
    print("╚" + "=" * 58 + "╝")
    print("\n")

    update_natnetsdk_findmodule()
    fix_fftw_cmake()

    print("\n" + "=" * 60)
    print("All additional fixes applied!")
    print("=" * 60)
    print("\nNote: Unicode definitions already removed from va_toolchain.cmake")
    print("\nNext steps:")
    print("  1. Clean build directory (optional but recommended)")
    print("  2. Re-run CMake configuration")
    print("  3. Build again")
    print("=" * 60)

if __name__ == "__main__":
    main()
