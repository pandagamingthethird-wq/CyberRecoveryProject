#ifndef CF_FSTAB_PARSER_H
#define CF_FSTAB_PARSER_H

#include <stdbool.h>

/*
 * fstab_parser — parses a real Android-format fstab (the 5-column
 * "device mountpoint fstype mountflags fsmgrflags" format, same as
 * recovery.fstab / the device's own fstab.<hardware>), replacing the
 * "you must name a partition device.conf already knows about" limitation
 * of mount_engine.c with actual auto-discovery.
 *
 * Honest scope: this parses the standard TREBLE-era fstab format and
 * extracts the fields CyberRecoveryProject's own engines care about
 * (mount point, device path, fs type, whether it's "logical"/encrypted).
 * It does NOT implement the full fs_mgr flag grammar (things like
 * zram, swap-specific flags, or avb-specific per-partition keys) —
 * unrecognized flags are preserved as raw text but not individually
 * parsed.
 */

#define CF_FSTAB_MAX_ENTRIES 32
#define CF_FSTAB_MAX_FIELD 128

typedef struct {
    char mount_point[CF_FSTAB_MAX_FIELD];
    char block_device[CF_FSTAB_MAX_FIELD];
    char fs_type[32];
    bool is_logical;     /* "logical" flag present — needs dm node created first */
    bool is_encryptable; /* "encryptable=" or "fileencryption=" present */
    char raw_flags[CF_FSTAB_MAX_FIELD]; /* full fsmgr flags column, unparsed */
} cf_fstab_entry_t;

typedef struct {
    cf_fstab_entry_t entries[CF_FSTAB_MAX_ENTRIES];
    int count;
} cf_fstab_t;

/* Parses the fstab at `path` into `out`. Returns true if at least one
 * valid entry was found. Malformed lines are skipped (not fatal) — a
 * single bad line in a real device fstab shouldn't take down every
 * other partition's mount info. */
bool cf_fstab_parse_file(const char *path, cf_fstab_t *out);

/* Same as above but parses an in-memory string (useful for testing
 * without a real file, and for parsing an fstab extracted from a ramdisk
 * already in memory). */
bool cf_fstab_parse_string(const char *content, cf_fstab_t *out);

/* Finds the entry whose mount_point matches `mount_point` (e.g.
 * "/system_image"). Returns NULL if not found. */
const cf_fstab_entry_t *cf_fstab_find(const cf_fstab_t *fstab, const char *mount_point);

#endif /* CF_FSTAB_PARSER_H */
