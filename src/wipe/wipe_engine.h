#ifndef CF_WIPE_ENGINE_H
#define CF_WIPE_ENGINE_H

#include <stdbool.h>

/*
 * wipe_engine — formats a partition (TWRP's "wipe" is a format, not a
 * file delete, for most partitions — matches user expectation of "wipe
 * cache" actually clearing corruption, not just removing visible files).
 *
 * Looks up the partition's block device and fs_type via fstab_parser
 * (auto-discovery) rather than requiring every partition to be named in
 * device.conf, so this works for any partition the fstab actually lists.
 */

/* Wipes `partition_name` (e.g. "cache", "data") by looking it up in the
 * fstab at `fstab_path`, then running the appropriate mkfs for its
 * fs_type. Returns false if the partition isn't found in the fstab, its
 * fs_type has no known mkfs mapping, or the format command fails. */
bool cf_wipe_partition(const char *partition_name, const char *fstab_path);

#endif /* CF_WIPE_ENGINE_H */
