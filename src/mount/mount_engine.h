#ifndef CF_MOUNT_ENGINE_H
#define CF_MOUNT_ENGINE_H

#include <stdbool.h>

/*
 * Mount/unmount, aware of logical (dynamic) partitions needing a
 * device-mapper node created first — this is exactly the class of bug
 * hit on BF7 ("Invalid Argument" on /system_image et al. from a missing
 * "logical" fstab flag / placeholder super partition size).
 *
 * Source of truth is the fstab (via fstab_parser.c), NOT device.conf's
 * PATH_* entries. This was changed after testing (tests/test_ors_execute.c)
 * caught mount_engine and wipe_engine resolving the SAME partition name
 * to TWO DIFFERENT block device paths — mount_engine was reading
 * device.conf's PATH_<NAME>, wipe_engine was reading the fstab, and on
 * BF7 those disagreed. The fstab is the real, device-authoritative
 * source (it's literally what the kernel/fs_mgr itself uses), so both
 * engines now read from it. device.conf is still used for values the
 * fstab doesn't carry (partition size presets, board name, etc.) — just
 * not for partition path resolution anymore.
 */

bool cf_mount_partition(const char *partition_name, const char *fstab_path);
bool cf_unmount_partition(const char *partition_name);

#endif /* CF_MOUNT_ENGINE_H */
