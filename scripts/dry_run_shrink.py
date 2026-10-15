#!/usr/bin/env python3
"""
dry_run_shrink.py — standalone test of CyberFlasher's shrink strategies
against a REAL unpacked BF7 ramdisk folder, with no dependency on the rest
of the project. Safe by default: reports what each strategy would do and
how many bytes it would free, but changes nothing on disk unless you pass
--apply.

Usage:
    python3 dry_run_shrink.py /path/to/unpacked_ramdisk_folder
    python3 dry_run_shrink.py /path/to/unpacked_ramdisk_folder --apply
    python3 dry_run_shrink.py /path/to/unpacked_ramdisk_folder --target-mb 32

Run this against the "ramdisk" folder you get from MIO-KITCHEN's unpack
(the decompressed, browsable one — not ramdisk.cpio.comp), same folder
you were already comparing against stock in your size investigation.

Requires: Python 3. Strategy 4 (strip symbols) needs `strip` on PATH
(part of binutils — on Windows, run this under WSL or wherever you have
your Linux toolchain, since that's also where magiskboot/repack happens).
"""

import argparse
import os
import shutil
import subprocess
import sys

KNOWN_DEBUG_PATTERNS = [
    "libclang_rt.ubsan_standalone-aarch64-android.so",
    "libasan.so",
    "strace",
    "gdbserver",
]

# From your earlier BF7 ramdisk comparison: concrete trim candidates that
# TWRP's default tree pulls in but a minimal recovery UI doesn't need.
KNOWN_UNUSED_EXTRAS = [
    "bin/bash",
    "bin/nano",
    "lib64/libssh.so",
]


def human(n: int) -> str:
    for unit in ["B", "KB", "MB", "GB"]:
        if abs(n) < 1024:
            return f"{n:.1f}{unit}"
        n /= 1024
    return f"{n:.1f}TB"


def dir_size(path: str) -> int:
    total = 0
    for root, _, files in os.walk(path):
        for f in files:
            fp = os.path.join(root, f)
            if os.path.islink(fp):
                continue
            try:
                total += os.path.getsize(fp)
            except OSError:
                pass
    return total


def find_matches(root: str, needles):
    matches = []
    for dirpath, _, files in os.walk(root):
        for f in files:
            full = os.path.join(dirpath, f)
            rel = os.path.relpath(full, root)
            for needle in needles:
                if needle in rel:
                    try:
                        size = os.path.getsize(full)
                    except OSError:
                        size = 0
                    matches.append((full, rel, size))
    return matches


def strategy_strip_debug_runtimes(root: str, apply: bool):
    matches = find_matches(root, KNOWN_DEBUG_PATTERNS)
    freed = sum(m[2] for m in matches)
    print(f"\n[1] Strip debug/sanitizer runtimes — {len(matches)} file(s), {human(freed)}")
    for full, rel, size in matches:
        print(f"      {rel}  ({human(size)})")
        if apply:
            os.remove(full)
    return freed


def strategy_strip_known_unused(root: str, apply: bool):
    matches = find_matches(root, KNOWN_UNUSED_EXTRAS)
    freed = sum(m[2] for m in matches)
    print(f"\n[2] Strip known-unused extras (bash/nano/libssh class) — "
          f"{len(matches)} file(s), {human(freed)}")
    for full, rel, size in matches:
        print(f"      {rel}  ({human(size)})")
        if apply:
            os.remove(full)
    print("      NOTE: this list is from your earlier manual ramdisk audit, "
          "not derived from menu_actions.c references (that check needs the "
          "real project build, not this standalone script) — eyeball this "
          "list before trusting it on anything you haven't already confirmed "
          "the UI doesn't call.")
    return freed


def strategy_strip_symbols(root: str, apply: bool):
    if not shutil.which("strip"):
        print("\n[3] Strip ELF symbols — SKIPPED (no `strip` on PATH; "
              "run under WSL/Linux toolchain)")
        return 0

    before = dir_size(root)
    candidates = []
    for dirpath, _, files in os.walk(root):
        for f in files:
            full = os.path.join(dirpath, f)
            if full.endswith(".so") or os.sep + "bin" + os.sep in full:
                candidates.append(full)

    print(f"\n[3] Strip ELF symbols — checking {len(candidates)} candidate file(s)")
    if apply:
        for full in candidates:
            subprocess.run(["strip", "--strip-unneeded", full],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        after = dir_size(root)
        freed = before - after
        print(f"      Freed: {human(freed)}")
        return freed
    else:
        print("      (dry-run: size impact only known after running with --apply, "
              "since `strip` savings vary per binary)")
        return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ramdisk_dir", help="Path to the unpacked ramdisk folder")
    ap.add_argument("--apply", action="store_true",
                     help="Actually delete/modify files (default: dry-run, report only)")
    ap.add_argument("--target-mb", type=float, default=None,
                     help="Target budget in MB, e.g. 32, to compare against")
    args = ap.parse_args()

    root = args.ramdisk_dir
    if not os.path.isdir(root):
        print(f"Not a directory: {root}", file=sys.stderr)
        sys.exit(1)

    start_size = dir_size(root)
    print(f"Current ramdisk folder size: {human(start_size)} ({start_size} bytes)")
    if args.target_mb:
        target_bytes = int(args.target_mb * 1024 * 1024)
        overage = start_size - target_bytes
        if overage > 0:
            print(f"Target: {args.target_mb}MB — currently {human(overage)} over budget")
        else:
            print(f"Target: {args.target_mb}MB — already within budget "
                  f"({human(-overage)} to spare)")

    mode = "APPLYING CHANGES" if args.apply else "DRY RUN (nothing will be modified)"
    print(f"\nMode: {mode}")

    total_freed = 0
    total_freed += strategy_strip_debug_runtimes(root, args.apply)
    total_freed += strategy_strip_known_unused(root, args.apply)
    total_freed += strategy_strip_symbols(root, args.apply)

    if args.apply:
        label = "Freed so far"
    else:
        label = "Estimated freeable (strategies 1-2 only; 3 needs --apply to measure)"
    print(f"\n{label}: {human(total_freed)}")

    if args.apply:
        end_size = dir_size(root)
        print(f"New folder size: {human(end_size)} ({end_size} bytes)")
        if args.target_mb:
            target_bytes = int(args.target_mb * 1024 * 1024)
            if end_size <= target_bytes:
                print("✅ Now within target budget.")
            else:
                print(f"⚠️  Still {human(end_size - target_bytes)} over target. "
                      "Remaining options: ramdisk recompression at max gzip level "
                      "(not done by this script — that happens at repack time), "
                      "or a language/theme/font trade-off.")
    else:
        print("\nRe-run with --apply to actually make these changes, "
              "ideally on a COPY of the ramdisk folder first.")


if __name__ == "__main__":
    main()
