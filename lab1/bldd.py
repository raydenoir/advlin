#!/usr/bin/env python3
"""bldd – Backward ldd

A command‑line utility that scans a directory tree for ELF executables and
reports which of them depend on which shared libraries (DT_NEEDED entries),
ordered by the number of executables that reference each library.

Features
========
* **Architecture‑aware**: results are grouped separately for x86 (i386), x86‑64,
  armv7 (ARM HF), and aarch64.
* **Configurable scan root** (``--scan-dir``) and **library filter**
  (``--libs``).
* **Multiple report formats**: plain text (default) or PDF (requires
  *reportlab*).
* Analysis is done with
  [`lief`](https://lief.quarkslab.com/) or, as fall‑back, the system tools
  ``readelf``/``objdump``.
* Clean ``--help`` output with usage examples.

Example
-------
::

    $ bldd --scan-dir ~/rootfs-aarch64 --format txt \
           --output report.txt
    Report written to report.txt

    $ cat report.txt
    bldd report – dynamic library usage in /home/user/rootfs-aarch64
    ---------------------------------------------------------------
    -------- aarch64 --------
    ld-linux-aarch64.so.1 (236 execs)
    -> /usr/bin/bash
    -> /usr/bin/coreutils
    [...]

"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import textwrap
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

try:
    import lief  # type: ignore

    _HAS_LIEF = True
except ImportError:  # pragma: no cover
    _HAS_LIEF = False

# ---------------------------------------------------------------------------
# Helpers & types
# ---------------------------------------------------------------------------
Arch = str  # convenience alias
Library = str
ExecutablePath = str

ARCH_NAMES: Dict[int, str] = {
    3: "i386 (x86)",          # EM_386
    62: "x86-64",             # EM_X86_64
    40: "armv7",              # EM_ARM
    183: "aarch64",           # EM_AARCH64
}

# ---------------------------------------------------------------------------
# ELF parsing front‑ends
# ---------------------------------------------------------------------------

def _libraries_with_lief(path: Path) -> Tuple[Arch | None, Sequence[Library]]:
    try:
        binary = lief.parse(str(path))
        
        if binary is None:
            print(f"{path}: lief.parse returned None")
            return None, []

        arch = ARCH_NAMES.get(int(binary.header.machine_type), None)
        return arch, list(binary.libraries)
        
    except Exception as e:
        print(f"{path}: Exception during parsing: {e}")
        return None, []



def _libraries_with_readelf(path: Path) -> Tuple[Arch | None, Sequence[Library]]:
    """Fallback parser using readelf/objdump when *lief* is unavailable."""
    # Discover architecture with readelf -h
    try:
        hdr = subprocess.check_output(["readelf", "-h", str(path)], text=True, stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None, []
    arch: Arch | None = None
    for line in hdr.splitlines():
        if "Machine:" in line:
            for key, val in ARCH_NAMES.items():
                if key.split("_")[1] in line or val in line:
                    arch = val
                    break
            break
    if arch is None:
        return None, []
    # Read DT_NEEDED entries via readelf -d
    try:
        dyn = subprocess.check_output(["readelf", "-d", str(path)], text=True, stderr=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        return None, []
    libs = []
    for line in dyn.splitlines():
        if "(NEEDED)" in line:
            parts = line.split("Shared library:")
            if len(parts) == 2:
                libs.append(parts[1].strip().strip("[]"))
    return arch, libs


# Choose parser implementation
libraries_for_binary = _libraries_with_lief if _HAS_LIEF else _libraries_with_readelf  # type: ignore[assignment]

# ---------------------------------------------------------------------------
# Main scan routine
# ---------------------------------------------------------------------------

def scan_directory(root: Path, lib_filter: Sequence[str] | None = None) -> Dict[Arch, Dict[Library, List[ExecutablePath]]]:
    """Walk *root* recursively and build a mapping arch→lib→[executables]."""
    mapping: Dict[Arch, Dict[Library, List[ExecutablePath]]] = defaultdict(lambda: defaultdict(list))
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        # Quick *executable* heuristic: x bit & ELF magic (0x7F 'ELF')
        try:
            if not os.access(path, os.X_OK):
                continue
            with path.open("rb") as fp:
                if fp.read(4) != b"\x7fELF":
                    continue
        except OSError:
            continue
        arch, libs = libraries_for_binary(path)
        if lib_filter:
            libs = [lib for lib in libs if any(pat in lib for pat in lib_filter)]
        for lib in libs:
            mapping[arch][lib].append(str(path))
    return mapping


# ---------------------------------------------------------------------------
# Report generation
# ---------------------------------------------------------------------------

def _make_text_report(mapping: Dict[Arch, Dict[Library, List[ExecutablePath]]], scan_root: Path) -> str:
    lines: List[str] = []
    header = f"bldd report – dynamic library usage in {scan_root}"
    lines.append(header)
    for arch in sorted(mapping.keys()):
        arch_hdr = f"----------  {arch}  ----------"
        lines.append(arch_hdr)
        libs_by_freq = sorted(mapping[arch].items(), key=lambda kv: len(kv[1]), reverse=True)
        for lib, exes in libs_by_freq:
            lines.append(f"{lib} ({len(exes)} execs)")
            for exe in exes:
                lines.append(f"    -> {exe}")
        lines.append("")
    return "\n".join(lines)


def _make_pdf_report(text: str, output: Path):
    """Render *text* into a simple PDF using reportlab."""
    try:
        from reportlab.lib.pagesizes import A4  # type: ignore
        from reportlab.pdfgen import canvas  # type: ignore
    except ImportError:  # pragma: no cover
        raise RuntimeError("reportlab is not installed; cannot output PDF")
    c = canvas.Canvas(str(output), pagesize=A4)
    width, height = A4
    margin = 40
    y = height - margin
    for line in text.split("\n"):
        if y < margin:
            c.showPage()
            y = height - margin
        c.drawString(margin, y, line)
        y -= 12  # 12‑pt line spacing
    c.save()


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    epilogue = textwrap.dedent(
        """
        Examples:
          Scan current directory and print to stdout:
            python3 bldd.py .

          Scan a rootfs and write PDF report:
            python3 bldd.py --scan-dir ~/rootfs-aarch64 --format pdf --output rootfs-report.pdf

          Focus on selected libraries only (e.g. libc):
            python3 bldd.py /usr --libs libc.so.6 ld-linux-x86-64.so.2
        """
    )
    p = argparse.ArgumentParser(
        prog="bldd",
        description="Backward ldd – list executables that depend on shared libraries",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=epilogue,
    )
    p.add_argument(
        "scan_dir_pos",
        nargs="?",
        default=None,
        help="Directory to scan recursively (default: current directory)",
    )
    p.add_argument(
        "--scan-dir",
        dest="scan_dir_opt",
        type=Path,
        help="Directory to scan recursively (alternative to positional argument)",
    )
    p.add_argument(
        "--libs",
        "-l",
        nargs="+",
        help="Filter: only include these library names (substring match)",
    )
    p.add_argument(
        "--format",
        "-f",
        choices=["txt", "pdf"],
        help="Report output format (txt | pdf)",
    )
    p.add_argument(
        "--output",
        "-o",
        type=Path,
        help="Write report to this file instead of stdout",
    )
    return p.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> None:
    args = _parse_args(argv)

    scan_root = args.scan_dir_opt or args.scan_dir_pos or "."
    scan_root = Path(scan_root).expanduser().resolve()
    if not scan_root.is_dir():
        sys.exit(f"Error: {scan_root} is not a directory")

    # Infer format from output extension if not explicitly provided
    output_path = args.output
    if args.format is None:
        if output_path and output_path.suffix.lower() == ".pdf":
            args.format = "pdf"
        else:
            args.format = "txt"

    mapping = scan_directory(scan_root, lib_filter=args.libs)
    report_txt = _make_text_report(mapping, scan_root)

    if args.format == "txt":
        if output_path:
            output_path.write_text(report_txt)
            print(f"Report written to {output_path}")
        else:
            print(report_txt)
    else:  # pdf
        if output_path is None:
            sys.exit("Error: --output is required when --format pdf")
        _make_pdf_report(report_txt, output_path)
        print(f"PDF report written to {output_path}")



if __name__ == "__main__":
    main()
