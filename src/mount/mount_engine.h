#ifndef CF_MOUNT_ENGINE_H
#define CF_MOUNT_ENGINE_H

#include <stdbool.h>

/* Mount/unmount, aware of logical (dynamic) partitions needing a
 * device-mapper node created first — this is exactly the class of bug
 * you hit on BF7 ("Invalid Argument" on /system_image et al. from a
 * missing "logical" fstab flag / placeholder super partition size).
 * CyberFlasher reads the "logical" flag and real sizes from device.conf
 * instead of a committed fstab, so a bad value is a one-line config fix,
 * not a tree edit + rebuild. */

bool cf_mount_partition(const char *partition_name, const char *device_conf_path);
bool cf_unmount_partition(const char *partition_name);

#endif /* CF_MOUNT_ENGINE_H */
