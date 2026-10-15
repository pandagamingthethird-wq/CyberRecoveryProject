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
