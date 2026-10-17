/*
 * flash_engine.c — unpack/repack/write. Deliberately thin: the real
 * unpack/repack work (boot header parsing, magiskboot-equivalent calls)
 * is device-family specific (e.g. the MTK 64-byte DTB header handling you
 * already reverse-engineered for the BF7). Keep that in a per-SoC backend
 * behind this same interface so core/ui code never changes per device.
 */

#include "flash_engine.h"
#include "backend_mtk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Reads MTK_DTB_HEADER_SIZE from device.conf; defaults to 64 (BF7's
 * confirmed value) if the key is absent so other devices without this
 * quirk aren't forced to set it. */
static int read_mtk_header_size(const char *device_conf_path) {
    FILE *f = fopen(device_conf_path, "r");
    if (!f) return 64;
    char line[256];
    int val = 64;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MTK_DTB_HEADER_SIZE=", 20) == 0) {
            val = atoi(line + 20);
            break;
        }
    }
    fclose(f);
    return val;
}

bool cf_flash_prepare_workdir(const char *image_path, char *workdir_out,
                               int workdir_out_cap, int64_t *size_out) {
    snprintf(workdir_out, workdir_out_cap, "/tmp/cf_unpack_%d", getpid());

    /* SoC backend selection currently hardcoded to MTK since that's the
     * confirmed/working path (BF7, MT6761). To support a non-MTK device,
     * read SOC from device.conf and branch to the matching backend_*.c —
     * same interface (unpack/repack), different header handling. */
    if (!mtk_unpack_boot_image(image_path, workdir_out,
                                read_mtk_header_size("config/device.conf"))) {
        return false;
    }

    char size_cmd[800];
    snprintf(size_cmd, sizeof(size_cmd), "du -sb '%s' 2>/dev/null | cut -f1", workdir_out);
    FILE *fp = popen(size_cmd, "r");
    if (!fp) return false;
    long long sz = 0;
    int ok = fscanf(fp, "%lld", &sz);
    pclose(fp);
    if (ok != 1) return false;
    *size_out = (int64_t)sz;
    return true;
}

bool cf_flash_repack_and_write(const char *workdir, const char *target_partition) {
    char out_img[600];
    snprintf(out_img, sizeof(out_img), "%s/new-boot.img", workdir);

    if (!mtk_repack_boot_image(workdir, out_img)) return false;

    char cmd[768];
    snprintf(cmd, sizeof(cmd), "dd if='%s' of='%s' bs=4096 2>/dev/null",
             out_img, target_partition);
    return system(cmd) == 0;
}
