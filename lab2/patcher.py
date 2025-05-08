#!/usr/bin/env python3
"""
patcher.py

Apply a bsdiff patch to an original binary, producing a patched executable.

Usage:
    ./patcher.py <original_binary> <patch_file> <output_binary>

Example:
    ./patcher.py hack_app hack_app.patch hack_app_patched
"""

import sys
import subprocess
import os
import stat

def apply_patch(orig_path: str, patch_path: str, out_path: str):
    try:
        subprocess.run(["bspatch", "-v"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except FileNotFoundError:
        sys.exit("ERROR: 'bspatch' not found. Please install the 'bsdiff' package.")

    print(f"Applying patch '{patch_path}' to '{orig_path}', writing '{out_path}'...")
    try:
        subprocess.run(
            ["bspatch", orig_path, out_path, patch_path],
            check=True
        )
    except subprocess.CalledProcessError as e:
        sys.exit(f"ERROR: bspatch failed: {e}")

    st = os.stat(out_path)
    os.chmod(out_path, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    print(f"Success: '{out_path}' is now patched and executable.")

def main():
    if len(sys.argv) != 4:
        print(__doc__)
        sys.exit(1)

    orig, patch, out = sys.argv[1], sys.argv[2], sys.argv[3]
    if not os.path.isfile(orig):
        sys.exit(f"ERROR: Original binary '{orig}' not found.")
    if not os.path.isfile(patch):
        sys.exit(f"ERROR: Patch file '{patch}' not found.")
    apply_patch(orig, patch, out)

if __name__ == "__main__":
    main()
