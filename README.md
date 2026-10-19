# CyberFlasher Recovery

A device-agnostic custom recovery skeleton with its own UI (not a TWRP/OrangeFox
reskin) and the same core action set: Flash, Backup, Restore, Wipe, Mount,
Advanced. The headline feature is **size-aware flashing**: the user selects a
target partition size (32MB / 64MB / 128MB / custom), and if the image they're
flashing exceeds that size, CyberFlasher automatically reduces it through a
layered strategy that preserves functionality — only falling back to asking
the user for a destructive trade-off (e.g. "remove extra languages?") when
automatic shrinking genuinely can't close the gap.

## Honest scope note

This is a **source-code architecture skeleton**, not a compiled, flashable
recovery binary. A real bootable recovery needs:
- A device tree + kernel/ramdisk for the target SoC (what your BF7 project
  already has)
- AOSP's `bootable/recovery` framework or an equivalent minimal init +
  graphics stack (minui/libpng/libjpeg or a custom framebuffer renderer)
- Device-specific fstab, partition layout, and boot image header handling

This skeleton is meant to sit *on top of* that per-device plumbing — the
`config/device.conf` file is where per-device values (partition sizes, paths,
board name) go, so the same core/ and ui/ code can be reused across devices
instead of being forked and hand-edited per device the way most TWRP trees are.

## Layout

```
CyberRecoveryProject/
├── config/
│   └── device.conf.example   # per-device values (board, partitions, sizes)
├── src/
│   ├── core/
│   │   ├── partition_size.c/.h   # detect/read target partition size
│   │   └── image_shrink.c/.h     # the auto-shrink engine (fully implemented)
│   ├── ui/
│   │   ├── ui_theme.c/.h          # custom look (not TWRP's style)
│   │   └── menu_actions.c/.h      # wires menu buttons to engines
│   ├── flash/
│   │   └── flash_engine.c/.h      # writes image to target partition
│   ├── backup/
│   │   └── backup_engine.c/.h     # nandroid-style backup/restore
│   └── mount/
│       └── mount_engine.c/.h      # mount/unmount partitions (A/B + logical aware)
├── docs/
│   └── ARCHITECTURE.md
├── scripts/
│   └── port_new_device.sh         # checklist/script for adapting to a new device
└── Makefile
```

## Porting to a new device

Everything device-specific lives in `config/device.conf`. Copy
`device.conf.example`, fill in the board name, available size presets, and
real partition paths, then rebuild. The UI and shrink logic don't need to
change per device.

## Current status: wired for BF7

`config/device.conf` is now filled in with your Tecno Spark Go 2023 (BF7)
values (MT6761, 32MB boot/recovery budget, confirmed super partition size,
gzip-only ramdisk). `src/flash/backend_mtk.c` implements the real unpack/
repack path for this device:

- Unpacks via `magiskboot`, then slices MTK's 64-byte wrapper off
  `dtb.img` (`MTK_DTB_HEADER_SIZE` in device.conf) before anything touches
  the real FDT.
- **Always regenerates `ramdisk.cpio.gz` from the unpacked `ramdisk/`
  folder on every repack** instead of trusting a cached `.comp` file —
  this directly targets the bug you hit where edits to the unpacked
  ramdisk silently didn't survive repack.
- Refuses non-gzip ramdisk compression for this device by default, since
  `/proc/config.gz` confirmed the kernel only has `CONFIG_RD_GZIP`.

With this wired in, flashing a boot/recovery image through CyberFlasher on
BF7 now runs: unpack → measure → if over 32MB, auto-strip debug/sanitizer
binaries (the `libclang_rt.ubsan_standalone-aarch64-android.so`-class bloat
you found) → strip menu-unreferenced binaries (`bin/bash`, `bin/nano`,
`libssh.so` would fall out here if nothing calls them) → recompress →
strip symbols → only then prompt for language/theme/font trade-offs →
repack → write. That's the ~1.1–2.9MB overage your CI builds have been
hitting, handled automatically instead of by hand-trimming the ramdisk
each time.

Not yet wired: `/data` FBE decrypt (still blocked on the `mcDrvDaemon`
TEE-daemon-missing-from-ramdisk issue, unrelated to this project's
architecture) and the `/system_image`-class logical mount fix (now
expressed via `USES_DYNAMIC_PARTITIONS` + `mount_engine.c`'s `dmctl create`
step, but untested against the real device yet).

## TWRP-parity additions (GUI, partition discovery, backup, OTA signing, ORS, sideload)

Closing specific gaps from a comparison against TeamWin's real
`bootable_recovery` source. Every item below was built AND tested during
development — not just written — except where explicitly marked.

| Subsystem | File(s) | Status |
|---|---|---|
| DataManager (GUI variable substitution) | `src/gui/data_manager.c` | ✅ Tested |
| Theme XML parser (subset, not general-purpose) | `src/gui/theme_xml.c` | ✅ Tested, incl. entity unescaping + malformed input |
| Framebuffer backend (fbdev) | `src/graphics/fb_backend.c` | ✅ Compiles against real kernel headers; **not runtime-tested** (no `/dev/fb0` in this sandbox) |
| Input backend (evdev) | `src/graphics/input_backend.c` | ✅ Tested — real decode state machine (down/move/up) fed synthetic `input_event` structs |
| Partition auto-discovery (fstab parser) | `src/partition/fstab_parser.c` | ✅ Tested against real BF7 `recovery.fstab` content |
| MD5 backup digest | `src/backup/twrp_digest.c` | ✅ Tested against official RFC 1321 vectors + cross-checked against system `md5sum` |
| File-level tar backup/restore (libarchive) | `src/backup/twrp_tar.c` | ⚠️ **NOT compile-tested** — this sandbox lacks `libarchive-dev` and network access to install it. Written against libarchive's stable documented API. CI now installs `libarchive-dev` and will catch any issue on the next push — treat this file as unverified until that run is green. |
| OTA zip signature verification | `src/ota/verify_ota_zip.py`, `sign_ota_zip.py` | ✅ **Tested end-to-end** against a real self-signed cert, including the actual attack scenario (zip signed by a different/untrusted key). **A real bug was caught and fixed during this process**: an initial `--no-chain` testing mode ignored which cert was passed entirely and would have accepted a maliciously-signed zip — see `tests/test_ota_verify.sh` for the regression test that catches this. Runs host-side (CI/workstation) via the system `openssl` CLI, not linked into the on-device binary yet — see that file's own header for why. |
| ORS-style scripting | `src/ors/openrecoveryscript.c` | ✅ Parser tested. Execution wiring to mount/backup/flash engines is straightforward but not yet written. |
| ADB sideload | `src/sideload/sideload_bridge.c` | ✅ Tested against a real FIFO. **This is explicitly NOT a custom `adbd`** — it's the receiving-side glue that assumes AOSP's actual `adbd --sideload` binary handles the USB/ADB protocol; reimplementing that protocol stack is out of scope and deliberately not attempted here. |
| Crypto (FBE/FDE decrypt) | `src/crypto/crypto_seam.h` | ❌ Deliberately unimplemented — header-only, documents the interface shape. Still blocked on the TEE daemon issue from the BF7 investigation; a stub that pretends to work would be worse than no implementation. |

Run `bash tests/run_tests.sh` for the C unit tests and
`bash tests/test_ota_verify.sh` for the OTA verifier's end-to-end test —
both now run automatically in CI on every push.

**Update — see the "Phases 0-4" section near the end of this README for
what's changed since the table above was written.** The short version:
OTA zip installer logic, ORS execution wiring, and real restore all now
exist and are tested — the table above is kept for historical context on
what the initial TWRP-parity pass covered, but treat the later section as
authoritative on current status.

Still missing relative to real TWRP: MTP/USB mass storage, a real theme
asset pipeline (`ui.zip` *loading* — the renderer/font pieces now exist,
see Phase 1 below), auto-discovery of input devices (you must still name
the `/dev/input/eventN` path), DRM/KMS graphics support (fbdev-only —
check which your actual recovery kernel exposes), and full Magisk.zip
compatibility specifically (the install *mechanism* is proven, but a real
Magisk installer script needs a bundled busybox/toybox this project
doesn't package yet — see Phase 2 below).

## ORS execution — wired to real engines, with one real bug found by testing

`cf_ors_execute()` (in `src/ors/openrecoveryscript.c`) now actually drives
`mount_engine`, `wipe_engine` (new — TWRP-style format, not TWRP-style
file-delete), `backup_engine`, and `data_manager`, instead of just parsing.
Tested end-to-end (`tests/test_ors_execute.c`) against fake `mount`/`umount`/
`mkfs.ext4` binaries on `PATH`, so the real command construction is verified
without needing root or a block device.

**That test surfaced a real inconsistency, since fixed (Phase 0)**:
`mount_engine.c` used to resolve a partition name via `device.conf`'s
`PATH_<NAME>` entries, while `wipe_engine.c` resolved the same name via the
fstab (through `fstab_parser`). For BF7 specifically these pointed at
*different* paths for the same logical partition — so `mount system` and
`wipe system` in the same ORS script could have operated on two different
block devices. **Fixed**: `mount_engine.c` now reads the fstab too (same
source of truth as `wipe_engine.c`), and `tests/test_ors_execute.c` has an
explicit regression assertion (not just "the script ran") that parses the
fake binaries' captured invocations and confirms `mount` and `wipe` resolve
"system" to the byte-identical block device path. `device.conf`'s `PATH_*`
entries are kept only for `backup_engine.c`'s future use (see its own TODO)
and human reference — they're no longer read for mount/wipe resolution.

`install` in an ORS script now verifies a zip's signature AND extracts +
executes its installer — see Phase 2 below for what changed and its honest
scope. It still requires `OTA_TRUSTED_CERT=<path>` in `device.conf`;
without it, `install` refuses to run rather than silently skipping
verification.

## Phases 0-4: from "architecture skeleton" to "proven install mechanism"

A later pass closed most of the gaps above, phase by phase, each with its
own real test — not just written, run. Every claim below has a
corresponding test file you can run yourself (`bash tests/run_tests.sh`
plus the standalone scripts named inline).

**Phase 0 — Fixed the mount/wipe inconsistency.** `mount_engine.c` now
reads the fstab (`fstab_parser.c`) instead of `device.conf`'s `PATH_*`
entries — same source of truth as `wipe_engine.c`. `device.conf`'s
`PATH_*` entries are no longer read for partition resolution at all (kept
only as human reference). `tests/test_ors_execute.c` has an explicit
regression assertion comparing the exact block device path `mount` and
`wipe` resolve "system" to.

**Phase 1 — Wired the UI into an actual render loop.** New:
`src/ui/ui_loop.c` (parses a theme page, renders buttons, dispatches taps
to bound action callbacks), `src/graphics/font5x7.c` (an original,
self-designed 5x7 bitmap font — not copied from any external source, to
avoid any licensing question) + `cf_fb_draw_text()` in `fb_backend.c`.
Tested via `tests/test_fb_text.c` (specific pixels checked against the
known glyph bitmaps) and `tests/test_ui_loop.c` (a canned synthetic touch
sequence correctly dispatches to the right button callbacks, ignores
off-button taps, and respects an event-count bound) — both run against an
in-memory framebuffer, no real `/dev/fb0` needed.

**Phase 2 — Made `install` actually install something.** This was the
actual blocker for "can I flash a ROM or Magisk.zip":
- `src/ota/ota_verify.c` — a **C port** of `verify_ota_zip.py`, so
  signature verification can run on-device instead of only host-side.
  Shells out to a bundled/system `openssl` binary rather than linking
  libcrypto directly (that cross-compile work isn't done). Tested against
  the **exact same 6 adversarial scenarios** as the Python version
  (`tests/test_ota_verify_c.sh`), including the malicious-signer case —
  porting logic that already survived adversarial testing, rather than
  writing new logic from scratch, was deliberate.
- `src/install/zip_installer.c` — extracts a zip (via `unzip`; a real
  on-device build needs a bundled busybox/toybox `unzip` applet or
  libarchive's zip-reading support instead), locates
  `META-INF/com/google/android/update-binary`, detects whether it's a
  shell script (Magisk's installer shape) or a self-contained ELF binary
  (most AOSP `update-binary`s), and executes it with the standard Android
  calling convention (`update-binary <api-version> <output-fd>
  <zip-path>`). **Genuinely tested end to end**
  (`tests/test_zip_installer.c`): a real synthetic zip, built with the
  system `zip` tool, containing a real shell script, extracted and
  executed through the real mechanism, confirmed via a marker file the
  script itself wrote. Same for the ELF-binary path, using a real
  compiled test binary. A real bug was caught here too: the extraction
  directory was originally named by PID alone, which collided across
  multiple `cf_zip_install()` calls within one process, leaking a
  previous install's files into a later one — fixed with `mkdtemp()` for
  genuine per-call uniqueness.
- **Honest scope**: this proves the mechanism (verify → extract → locate
  → detect type → exec with correct argv → capture result), not
  compatibility with real-world Magisk.zip specifically, which calls
  busybox applets throughout its script and needs one bundled in the
  ramdisk — a packaging step, not a logic gap, and not done here. An AOSP
  zip whose `update-binary` is a self-contained ELF binary doing its own
  Edify interpretation is the better-covered case.

**`tests/test_e2e_install.c`/`.sh` is the capstone**: a real `install
<zip>` ORS command, parsed and executed through `cf_ors_execute()`,
against a real signed synthetic zip — proving the full chain (ORS parse →
execute → C signature verify → zip extract → installer exec) works
together, plus confirming a maliciously-signed zip is rejected *before*
the installer ever runs (checked by confirming its marker file never gets
written).

**Phase 3 — Restore is no longer a no-op; system/vendor backup is real.**
`backup_engine.c` was rewritten: boot/recovery stay whole-partition `dd`
(flat images), but system/vendor now use `src/backup/twrp_tar.c`
(libarchive-based file-level tar) instead of not being backed up at all.
Every backed-up file gets an MD5 `.md5` sidecar (`twrp_digest.c`).
`cf_restore_run()` now actually restores: it verifies each file's digest
**before** touching anything and refuses the whole restore if any file
fails that check, rather than writing back something possibly corrupted.
Partition paths come from the fstab (same Phase 0 fix, applied here too).

`twrp_tar.c` itself — previously flagged as **never compile-tested** —
is now genuinely compile-and-link-tested
(`tests/test_twrp_tar.sh`/`test_backup_restore.sh`) against a real
installed `libarchive`, including a full create→extract round trip with
nested directories. In the sandbox this development happened in, no
network access meant no `libarchive-dev` package, so a minimal shim
header (`tests/archive_shim/`, matching libarchive's real stable ABI for
just the functions used) was linked against the actual installed
`libarchive.so` instead — clearly marked as a **fallback only**; the test
scripts prefer real `libarchive-dev` headers when present (as CI now
has). Two real bugs were caught and fixed in `twrp_tar.c` during this:
dead code referencing nonexistent `struct FTW` members, and a missing
`_XOPEN_SOURCE` feature-test macro that `nftw()`/`FTW_PHYS` need.
`tests/test_backup_restore.sh` proves a full round trip against synthetic
partition files (regular files standing in for block devices) AND a
synthetic mounted filesystem, including correctly **refusing** to restore
when a backup's `.md5` sidecar has been tampered with.

**Phase 4 — Regression coverage.** Every test above runs in
`bash tests/run_tests.sh` (the C suite) or its own standalone script, and
all of it runs in CI (`.github/workflows/build.yml`) on every push —
`libarchive-dev`, `unzip`, and `zip` are installed there alongside the
existing toolchain.

## Power menu

`src/power/power_menu.c` — Power Off, Reboot, Reboot to Recovery, Reboot
to Fastboot/Bootloader. Uses the real Linux `reboot(2)` syscall with
Android's standard magic numbers and reboot-reason strings (same values
`libcutils`' `android_reboot()` uses) — `sync()`s filesystems first.
Tested (`tests/test_power_menu.c`) via an injected fake syscall that
records the call instead of actually rebooting the test machine, checking
every action produces the exact right magic/cmd/reason combination, plus
that a syscall failure correctly propagates as a `false` return.

**"Reboot to EDL" does not apply to this device.** EDL (Emergency
Download Mode) is Qualcomm-specific; BF7 is MediaTek MT6761. The closest
MTK equivalent is BROM/Preloader download mode
(`CF_POWER_REBOOT_DOWNLOAD`), which this attempts via the same software
reboot-reason mechanism (`"download"`) — but unlike EDL, this is **not
confirmed to work on this specific BF7 unit**, and MTK's software trigger
is far less standardized across devices/preloader versions than
Qualcomm's. The known-working fallback is the physical button combo
(Volume Up + Power while connecting USB with the device off). Don't treat
`CF_POWER_REBOOT_DOWNLOAD` as equivalent in reliability to the other four
actions.

Wired into the UI via `src/ui/menu_actions.h`'s callback wrappers
(`cf_action_poweroff`, `cf_action_reboot`, `cf_action_reboot_recovery`,
`cf_action_reboot_bootloader`, `cf_action_reboot_download`) — see that
file's own header comment for an example theme page XML snippet. These
wrapper functions are intentionally thin and not separately tested
(calling them for real would reboot/power off the machine); the two
pieces they connect — `cf_power_execute`'s dispatch and `ui_loop`'s tap
dispatch — are each tested on their own.
