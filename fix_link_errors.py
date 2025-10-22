#!/usr/bin/env python3
"""
VA2022a Link Error Fixes
Fixes discovered during INSTALL target:
1. FFTW m.lib dependency issue (remove math library references)
2. NatNetSDK sprintf/sscanf linking issues (add legacy_stdio_definitions.lib)
"""

import os
import re
from pathlib import Path

BASE_PATH = Path("D:/Project/VA-build")

def fix_fftw_cmake():
    """Fix FFTW m.lib issue by removing math library dependency"""
    print("=" * 60)
    print("Fix 1: Removing m.lib dependency from FFTW...")
    print("=" * 60)

    file_path = BASE_PATH / "build/_deps/fftw-src/CMakeLists.txt"

    if not file_path.exists():
        print(f"  [WARN] File not found: {file_path}")
        return

    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Remove MATHLIB variable references completely
    modified = re.sub(r'\$\{MATHLIB\}', '', content)
    modified = re.sub(r'set\s*\(\s*MATHLIB\s+m\s*\)', '', modified, flags=re.IGNORECASE)
    modified = re.sub(r'list\s*\(\s*APPEND\s+\w+\s+\$\{MATHLIB\}\s*\)', '', modified, flags=re.IGNORECASE)

    if content != modified:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(modified)
        print(f"  [OK] Fixed FFTW CMakeLists.txt")
    else:
        print(f"  - No changes needed")

def fix_natnet_sprintf_issue():
    """Fix NatNetSDK sprintf/sscanf linking issues by adding legacy_stdio_definitions.lib"""
    print("\n" + "=" * 60)
    print("Fix 2: Adding legacy_stdio_definitions.lib for NatNetSDK...")
    print("=" * 60)

    # Fix IHTATracking CMakeLists.txt
    file_path = BASE_PATH / "build/_deps/ihtautilitylibs-src/IHTATracking/CMakeLists.txt"

    if not file_path.exists():
        print(f"  [WARN] File not found: {file_path}")
        return

    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Add legacy_stdio_definitions.lib to target_link_libraries
    if 'legacy_stdio_definitions' not in content:
        # Find the target_link_libraries command for IHTATracking
        pattern = r'(target_link_libraries\s*\(\s*\$\{PROJECT_NAME\}[^)]+)'

        def add_legacy_lib(match):
            return match.group(1) + '\n    $<$<PLATFORM_ID:Windows>:legacy_stdio_definitions>'

        modified = re.sub(pattern, add_legacy_lib, content, flags=re.DOTALL)

        if content != modified:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(modified)
            print(f"  [OK] Added legacy_stdio_definitions.lib to IHTATracking")
        else:
            print(f"  - Could not find target_link_libraries, trying alternative approach")
            # Alternative: append at the end before last line
            lines = content.split('\n')
            for i, line in enumerate(lines):
                if 'target_link_libraries' in line and 'IHTATracking' in content[max(0,i-10):i+20]:
                    # Find the closing parenthesis
                    j = i
                    while j < len(lines) and ')' not in lines[j]:
                        j += 1
                    if j < len(lines):
                        lines[j] = '    $<$<PLATFORM_ID:Windows>:legacy_stdio_definitions>\n' + lines[j]
                        modified = '\n'.join(lines)
                        with open(file_path, 'w', encoding='utf-8') as f:
                            f.write(modified)
                        print(f"  [OK] Added legacy_stdio_definitions.lib (alternative method)")
                        break
    else:
        print(f"  - Already contains legacy_stdio_definitions")

def main():
    print("\n")
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 58 + "║")
    print("║" + "  VA2022a Link Error Fixes".center(58) + "║")
    print("║" + " " * 58 + "║")
    print("╚" + "=" * 58 + "╝")
    print("\n")

    fix_fftw_cmake()
    fix_natnet_sprintf_issue()

    print("\n" + "=" * 60)
    print("All link error fixes applied!")
    print("=" * 60)
    print("\nNext steps:")
    print("  1. Re-run: cmake --build . --config Release -j8")
    print("  2. Then run: cmake --build . --config Release --target INSTALL -j8")
    print("=" * 60)

if __name__ == "__main__":
    main()
