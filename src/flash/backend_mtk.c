#include "backend_mtk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static bool run(const char *fmt, ...) {
    char cmd[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    return system(cmd) == 0;
}

/*
 * magiskboot already understands the MTK kernel/ramdisk headers, so we lean
 * on it for the split, then separately re-slice dtb.img/dtbo.img to strip
 * MTK's 64-byte wrapper, since magiskboot doesn't touch those.
 */
bool mtk_unpack_boot_image(const char *image_path, const char *workdir,
                            int mtk_dtb_header_size) {
    if (!run("mkdir -p '%s'", workdir)) return false;
    if (!run("cd '%s' && magiskboot unpack -h '%s' > unpack.log 2>&1; "
              "magiskboot unpack '%s' >> unpack.log 2>&1",
              workdir, image_path, image_path)) {
        fprintf(stderr, "magiskboot unpack failed, see %s/unpack.log\n", workdir);
        return false;
    }

    /* Keep the ORIGINAL compressed ramdisk around too (not just the
     * magiskboot-decompressed ramdisk.cpio), mirroring the MIO-KITCHEN
     * "don't decompress while unpack" fix that avoided the stale-size
     * confusion seen earlier. */
    run("cp '%s/ramdisk.cpio' '%s/ramdisk.cpio.orig' 2>/dev/null", workdir, workdir);

    /* Slice the 64-byte MTK header off dtb.img if present, so downstream
     * tooling (dtc, size accounting) sees the real FDT, not MTK's wrapper. */
    char dtb_path[512];
    snprintf(dtb_path, sizeof(dtb_path), "%s/dtb.img", workdir);
    FILE *f = fopen(dtb_path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long total = ftell(f);
        fclose(f);
        if (total > mtk_dtb_header_size) {
            run("tail -c +%d '%s' > '%s.real' && mv '%s.real' '%s'",
                mtk_dtb_header_size + 1, dtb_path, dtb_path, dtb_path, dtb_path);
        }
    }
    return true;
}

bool mtk_regenerate_ramdisk(const char *workdir) {
    char cpio[512], comp[512];
    snprintf(cpio, sizeof(cpio), "%s/ramdisk.cpio", workdir);
    snprintf(comp, sizeof(comp), "%s/ramdisk.cpio.gz", workdir);

    /* Always rebuild the cpio from the unpacked ramdisk/ folder, then
     * gzip -9 it fresh. This is the explicit fix for the earlier bug where
     * edits to the unpacked folder weren't reflected after repack: repack
     * was reusing a stale cached .comp file instead of regenerating. */
    char ramdisk_dir[512];
    snprintf(ramdisk_dir, sizeof(ramdisk_dir), "%s/ramdisk", workdir);
    if (!run("cd '%s' && find . | cpio -o -H newc > '%s' 2>/dev/null",
              ramdisk_dir, cpio)) {
        return false;
    }
    return run("gzip -9 -f -c '%s' > '%s'", cpio, comp);
}

bool mtk_repack_boot_image(const char *workdir, const char *out_image_path) {
    /* Force a fresh ramdisk build before repack every time — never trust
     * whatever's cached, which is what caused the "edits don't stick"
     * issue before. */
    if (!mtk_regenerate_ramdisk(workdir)) return false;

    return run("cd '%s' && magiskboot repack original '%s' 2>repack.log",
                workdir, out_image_path);
}
