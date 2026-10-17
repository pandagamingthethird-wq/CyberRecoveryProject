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

**Still missing relative to real TWRP** (unchanged from the earlier gap
analysis): OTA zip *installer* logic (parsing `updater-script`/Edify and
actually running the install — signature verification above is a
necessary first step, not the whole thing), MTP/USB mass storage, a real
theme asset pipeline (`ui.zip` loading, font rendering), auto-discovery
of input devices (currently you must name the `/dev/input/eventN` path),
and DRM/KMS graphics support (fbdev-only currently — check which your
actual recovery kernel exposes).

## ORS execution — wired to real engines, with one real bug found by testing

`cf_ors_execute()` (in `src/ors/openrecoveryscript.c`) now actually drives
`mount_engine`, `wipe_engine` (new — TWRP-style format, not TWRP-style
file-delete), `backup_engine`, and `data_manager`, instead of just parsing.
Tested end-to-end (`tests/test_ors_execute.c`) against fake `mount`/`umount`/
`mkfs.ext4` binaries on `PATH`, so the real command construction is verified
without needing root or a block device.

**That test surfaced a real inconsistency, not a hypothetical one**:
`mount_engine.c` resolves a partition name via `device.conf`'s `PATH_<NAME>`
entries, while `wipe_engine.c` resolves the same name via the fstab (through
`fstab_parser`). For BF7 specifically these currently point at *different*
paths for the same logical partition (`device.conf`'s `PATH_system` vs. the
fstab's by-name entry for `/system`) — so `mount system` and `wipe system`
in the same ORS script could operate on two different block devices. This
needs resolving (most likely: make `mount_engine` use `fstab_parser` too,
so there's one source of truth) before trusting any script that both mounts
and wipes the same partition. Flagging this now rather than letting a test
that "passed" hide a real correctness gap.

`install` in an ORS script currently verifies a zip's signature via
`verify_ota_zip.py` and nothing more — see that command's own code comment
for why (no installer exists yet). It requires `OTA_TRUSTED_CERT=<path>` in
`device.conf`; without it, `install` refuses to run rather than silently
skipping verification.
