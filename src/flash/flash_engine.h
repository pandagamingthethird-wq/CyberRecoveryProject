#ifndef CF_FLASH_ENGINE_H
#define CF_FLASH_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

/* Unpacks image_path (kernel/ramdisk/dtb split via magiskboot-equivalent)
 * into a fresh scratch directory, writes its path into workdir_out, and
 * reports the unpacked tree's total size (what image_shrink.c operates on). */
bool cf_flash_prepare_workdir(const char *image_path, char *workdir_out,
                               int workdir_out_cap, int64_t *size_out);

/* Repacks the (possibly shrunk) workdir back into a boot image and writes
 * it to target_partition's block device (same effect as TWRP's flash
 * action — fastboot-style "flash" semantics, not a reboot trigger). */
bool cf_flash_repack_and_write(const char *workdir, const char *target_partition);

#endif /* CF_FLASH_ENGINE_H */
