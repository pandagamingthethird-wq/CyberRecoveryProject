#include "wipe_engine.h"
#include "../partition/fstab_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *fs_type;
    const char *mkfs_cmd_fmt; /* %s = block device */
} mkfs_mapping_t;

static const mkfs_mapping_t MKFS_MAP[] = {
    { "ext4", "mkfs.ext4 -F '%s'" },
    { "f2fs", "mkfs.f2fs -f '%s'" },
    { "vfat", "mkfs.vfat '%s'" },
};
#define MKFS_MAP_COUNT (int)(sizeof(MKFS_MAP) / sizeof(MKFS_MAP[0]))

bool cf_wipe_partition(const char *partition_name, const char *fstab_path) {
    cf_fstab_t fstab;
    if (!cf_fstab_parse_file(fstab_path, &fstab)) {
        fprintf(stderr, "cf_wipe_partition: could not parse fstab at %s\n", fstab_path);
        return false;
    }

    /* ORS/TWRP convention: commands use the short name ("cache"), fstab
     * uses the mount point ("/cache") — try with a leading slash first,
     * then fall back to an exact match in case the caller already
     * passed a full mount point. */
    char with_slash[256];
    snprintf(with_slash, sizeof(with_slash), "/%s", partition_name);

    const cf_fstab_entry_t *entry = cf_fstab_find(&fstab, with_slash);
    if (!entry) entry = cf_fstab_find(&fstab, partition_name);
    if (!entry) {
        fprintf(stderr, "cf_wipe_partition: partition '%s' not found in fstab\n", partition_name);
        return false;
    }

    const char *mkfs_fmt = NULL;
    for (int i = 0; i < MKFS_MAP_COUNT; i++) {
        if (strcmp(entry->fs_type, MKFS_MAP[i].fs_type) == 0) {
            mkfs_fmt = MKFS_MAP[i].mkfs_cmd_fmt;
            break;
        }
    }
    if (!mkfs_fmt) {
        fprintf(stderr, "cf_wipe_partition: no mkfs mapping for fs_type '%s' "
                "(partition '%s')\n", entry->fs_type, partition_name);
        return false;
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), mkfs_fmt, entry->block_device);
    printf("cf_wipe_partition: %s\n", cmd);
    return system(cmd) == 0;
}
