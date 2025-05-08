#!/usr/bin/env python3
import sys, hashlib

def make_license(hwid: str) -> str:
    """
    hwid: 16-char uppercase hex string.
    Returns: 32-char lowercase hex MD5-reversed license.
    """
    if len(hwid) != 16 or any(c not in "0123456789ABCDEF" for c in hwid):
        sys.exit("ERROR: HWID must be 16 uppercase hex chars.")
    # MD5 digest of ASCII-encoded HWID
    d = hashlib.md5(hwid.encode('ascii')).digest()
    # reverse the bytes, then output lowercase hex
    return d[::-1].hex()

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <16-char HWID>")
        sys.exit(1)
    print(make_license(sys.argv[1]))

if __name__ == "__main__":
    main()
