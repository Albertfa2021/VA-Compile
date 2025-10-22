#!/usr/bin/env python3
"""
Fix EVDLAlgorithm enum type errors in VACore
Changes int type declarations to EVDLAlgorithm type
"""

import re
import os

def fix_file(filepath, fixes):
    """Apply regex fixes to a file"""
    print(f"\nProcessing: {filepath}")

    if not os.path.exists(filepath):
        print(f"  ERROR: File not found!")
        return False

    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    original_content = content
    changes_made = 0

    for old_pattern, new_text, description in fixes:
        matches = re.findall(old_pattern, content)
        if matches:
            content = re.sub(old_pattern, new_text, content)
            changes_made += len(matches)
            print(f"  OK {description}: {len(matches)} occurrence(s)")

    if content != original_content:
        with open(filepath, 'w', encoding='utf-8', newline='\n') as f:
            f.write(content)
        print(f"  SUCCESS: {changes_made} changes made")
        return True
    else:
        print(f"  No changes needed")
        return False

# File paths
base_path = r"D:\Project\VA-build\source\VACore\src\Rendering"

files_to_fix = {
    # Base class header - root cause
    os.path.join(base_path, "Base", "VAAudioRendererBase.h"): [
        (r'\bint\s+iVDLSwitchingAlgorithm\s*=\s*-1;',
         'EVDLAlgorithm iVDLSwitchingAlgorithm = EVDLAlgorithm::SWITCH;',
         'Change iVDLSwitchingAlgorithm type from int to EVDLAlgorithm'),
    ],

    # BinauralFreeField header
    os.path.join(base_path, "Binaural", "FreeField", "VABinauralFreefieldAudioRenderer.h"): [
        (r'\bint\s+m_iDefaultVDLSwitchingAlgorithm;',
         'EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm;',
         'Change m_iDefaultVDLSwitchingAlgorithm type from int to EVDLAlgorithm'),
    ],

    # BinauralFreeField source
    os.path.join(base_path, "Binaural", "FreeField", "VABinauralFreeFieldAudioRenderer.cpp"): [
        (r'\bint\s+m_iDefaultVDLSwitchingAlgorithm;',
         'EVDLAlgorithm m_iDefaultVDLSwitchingAlgorithm;',
         'Change m_iDefaultVDLSwitchingAlgorithm type from int to EVDLAlgorithm in struct'),
    ],
}

print("=" * 80)
print("Fixing EVDLAlgorithm enum type errors")
print("=" * 80)

total_files = 0
for filepath, fixes in files_to_fix.items():
    if fix_file(filepath, fixes):
        total_files += 1

print("\n" + "=" * 80)
print(f"Fix completed: {total_files} file(s) modified")
print("=" * 80)
