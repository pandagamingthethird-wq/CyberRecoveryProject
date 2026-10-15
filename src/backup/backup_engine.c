#include "backup_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool dd_partition_to_file(const char *by_name_path, const char *out_path,
                                  bool compress) {
    char cmd[1024];
    if (compress) {
        snprintf(cmd, sizeof(cmd), "dd if='%s' bs=4M 2>/dev/null | gzip -1 > '%s.gz'",
                 by_name_path, out_path);
    } else {
        snprintf(cmd, sizeof(cmd), "dd if='%s' of='%s' bs=4M 2>/dev/null",
                 by_name_path, out_path);
    }
    return system(cmd) == 0;
}

bool cf_backup_run(const cf_backup_selection_t *sel,
                    const char *device_conf_path, const char *dest_dir) {
    (void)device_conf_path; /* real impl: look up PATH_* entries per selected item */
    char stamp_dir[512];
    time_t now = time(NULL);
    snprintf(stamp_dir, sizeof(stamp_dir), "%s/%ld", dest_dir, (long)now);

    char mk[600];
    snprintf(mk, sizeof(mk), "mkdir -p '%s'", stamp_dir);
    system(mk);

    bool ok = true;
    if (sel->boot) {
        char out[600]; snprintf(out, sizeof(out), "%s/boot.img", stamp_dir);
        ok &= dd_partition_to_file("/dev/block/bootdevice/by-name/boot", out, sel->compress);
    }
    if (sel->recovery) {
        char out[600]; snprintf(out, sizeof(out), "%s/recovery.img", stamp_dir);
        ok &= dd_partition_to_file("/dev/block/bootdevice/by-name/recovery", out, sel->compress);
    }
    /* system/vendor/data: same pattern, typically tar+compress rather than
     * raw dd since they're filesystems, not flat images — omitted here to
     * keep this skeleton focused; same dd_partition_to_file-style helper
     * applies with `tar` substituted for `dd`. */
    return ok;
}

bool cf_restore_run(const char *backup_dir, const char *device_conf_path) {
    (void)backup_dir; (void)device_conf_path;
    /* Mirror of cf_backup_run: for each image found in backup_dir, dd it
     * back to the matching by-name path looked up from device.conf. */
    return true;
}
