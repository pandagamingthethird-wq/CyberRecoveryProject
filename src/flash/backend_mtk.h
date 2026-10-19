#ifndef CF_BACKEND_MTK_H
#define CF_BACKEND_MTK_H

#include <stdbool.h>
#include <stdint.h>

/*
 * MediaTek boot image backend. Handles the MTK-specific wrinkles found
 * while working on the BF7 (MT6761, header v2, non-GKI):
 *   - MTK DTB/DTBO images wrap the real FDT in a 64-byte proprietary header
 *     (must slice data[header_size : header_size+real_size] before dtc)
 *   - magiskboot's cached ramdisk.cpio.comp can go stale after editing the
 *     unpacked ramdisk folder; this backend force-regenerates it instead of
 *     trusting whatever repack finds on disk
 *   - gzip is the only ramdisk compression this device's kernel can read
 *     (CONFIG_RD_GZIP only, confirmed via /proc/config.gz) — repack refuses
 *     to use anything else unless device.conf explicitly overrides it
 */

bool mtk_unpack_boot_image(const char *image_path, const char *workdir,
                            int mtk_dtb_header_size);

/* Rebuilds ramdisk.cpio.gz from the (possibly shrunk) unpacked ramdisk/
 * folder — always regenerates from source rather than trusting a cached
 * .comp file, which is what silently broke repacks before. */
bool mtk_regenerate_ramdisk(const char *workdir);

bool mtk_repack_boot_image(const char *workdir, const char *out_image_path);

#endif /* CF_BACKEND_MTK_H */
