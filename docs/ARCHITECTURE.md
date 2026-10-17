# Architecture

## Layering

```
ui/           -> menu rendering + wiring buttons to engines (device-agnostic)
core/         -> partition size presets, the auto-shrink engine (device-agnostic)
flash/        -> unpack/repack/write (thin here; device-family backend plugs in)
backup/       -> nandroid-style backup/restore (device-agnostic, paths from config)
mount/        -> mount/unmount, logical-partition aware (device-agnostic)
config/       -> device.conf — the ONLY place device-specific values live
```

The goal of this split: your BF7 work shows the actual pain points of a
single-device TWRP tree — hardcoded `BoardConfig.mk` values, a committed
`recovery.fstab` that has to be hand-fixed per device, size budgets baked
into Make variables. CyberFlasher pushes all of that into `device.conf` so
the same compiled core logic ports by editing one text file, not by forking
and patching the whole tree.

## What still needs a real device port

1. **Boot image unpack/repack backend** (`flash_engine.c`'s TODO): for
   MediaTek devices like the BF7 this means handling the 64-byte proprietary
   DTB header before handing data to `dtc`, same as you already reverse
   engineered. For other SoCs, the generic AOSP boot header or
   vendor_boot/GKI path applies instead. This is a `#ifdef`/strategy-pattern
   boundary, not a core-logic change.
2. **Graphics backend** for `ui_theme.c`: minui, a raw fbdev writer, or
   DRM/KMS depending on what the device's kernel exposes.
3. **Init integration**: the compiled binary needs to be the thing recovery
   ramdisk's `init.rc` launches as the UI process, same role TWRP's own
   binary plays.
4. **TEE/keymaster for FBE decrypt** (if you want `/data` decryption in
   recovery): this is the same `mcDrvDaemon`-not-connecting issue you're
   already debugging on BF7 — it's a ramdisk-contents problem (missing TEE
   daemon/init service), not something this skeleton's architecture affects
   either way.

## Why the shrink engine is layered the way it is

Strategies in `image_shrink.c` are ordered strip-debug-stuff →
strip-unreferenced-binaries → recompress → strip-symbols specifically
because each is **zero functional impact**, attempted in order from
cheapest/safest to more involved, entirely before the user is ever asked
anything. Only once all four are exhausted does it build a trade-off
catalogue (extra languages, theme assets, fonts) and hand control to the UI
callback — so "ask the user if there's no other choice" is enforced by
the call order in `cf_shrink_image_to_fit`, not by a flag the UI has to
remember to check.
