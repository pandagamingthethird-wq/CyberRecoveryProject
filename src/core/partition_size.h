#ifndef CF_PARTITION_SIZE_H
#define CF_PARTITION_SIZE_H

#include <stdint.h>

typedef struct {
    int64_t bytes;
    char label[16];   /* "32MB", "64MB", "Custom" */
} cf_size_preset_t;

/* Loads PARTITION_SIZE_PRESETS / PARTITION_SIZE_LABELS from device.conf.
 * Returns the number of presets loaded into `out` (caller-allocated,
 * capacity `max`). */
int cf_load_size_presets(const char *device_conf_path,
                          cf_size_preset_t *out, int max);

/* Reads the real on-device partition size for `partition_name` (e.g.
 * "recovery", "boot") via /proc/partitions or BLKGETSIZE64 ioctl on the
 * by-name block device path from device.conf — used to sanity-check the
 * user's chosen preset against what the device can actually hold. */
int64_t cf_get_actual_partition_size(const char *by_name_path);

#endif /* CF_PARTITION_SIZE_H */
