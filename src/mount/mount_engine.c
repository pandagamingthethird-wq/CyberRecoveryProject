#include "mount_engine.h"
#include "../partition/fstab_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Looks up `partition_name` in the fstab (same "/" + name first, then
 * exact match" convention wipe_engine.c uses, so both engines resolve a
 * given ORS/CLI partition name identically). For logical partitions
 * (fstab "logical" flag), creates the device-mapper node before mount. */
bool cf_mount_partition(const char *partition_name, const char *fstab_path) {
    cf_fstab_t fstab;
    if (!cf_fstab_parse_file(fstab_path, &fstab)) {
        fprintf(stderr, "cf_mount_partition: could not parse fstab at %s\n", fstab_path);
        return false;
    }

    char with_slash[256];
    snprintf(with_slash, sizeof(with_slash), "/%s", partition_name);

    const cf_fstab_entry_t *entry = cf_fstab_find(&fstab, with_slash);
    if (!entry) entry = cf_fstab_find(&fstab, partition_name);
    if (!entry) {
        fprintf(stderr, "cf_mount_partition: partition '%s' not found in fstab\n", partition_name);
        return false;
    }

    if (entry->is_logical) {
        char dm_cmd[512];
        snprintf(dm_cmd, sizeof(dm_cmd), "dmctl create %s 2>/dev/null", partition_name);
        system(dm_cmd); /* best-effort: node may already exist */
    }

    char mount_point[300];
    snprintf(mount_point, sizeof(mount_point), "/mnt/%s", partition_name);
    char mkdir_cmd[350];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", mount_point);
    system(mkdir_cmd);

    char mount_cmd[700];
    snprintf(mount_cmd, sizeof(mount_cmd), "mount -t '%s' '%s' '%s'",
             entry->fs_type, entry->block_device, mount_point);
    printf("cf_mount_partition: %s\n", mount_cmd);
    return system(mount_cmd) == 0;
}

bool cf_unmount_partition(const char *partition_name) {
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "umount '/mnt/%s' 2>/dev/null", partition_name);
    return system(cmd) == 0;
}
