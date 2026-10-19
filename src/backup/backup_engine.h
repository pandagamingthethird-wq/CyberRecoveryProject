#ifndef CF_BACKUP_ENGINE_H
#define CF_BACKUP_ENGINE_H

#include <stdbool.h>

/*
 * Nandroid-style backup/restore — same semantics as TWRP's backup menu.
 *
 * boot/recovery: whole-partition dd (they're flat images, not
 * filesystems — dd is the right tool). system/vendor: file-level tar
 * via twrp_tar.c (they're real filesystems — tar lets you restore
 * selectively and doesn't care about exact partition size on restore,
 * unlike a raw dd image). Every backed-up file gets an MD5 .md5 sidecar
 * (twrp_digest.c) written alongside it; restore refuses to proceed if a
 * file fails its digest check, rather than writing back something that
 * might be corrupted.
 *
 * Partition block device paths come from the fstab (fstab_parser.c) —
 * same source of truth as mount_engine.c and wipe_engine.c after the
 * Phase 0 fix, so backup/restore can't disagree with mount/wipe about
 * where a partition actually lives.
 *
 * system/vendor backup assumes the partition is ALREADY MOUNTED at
 * /mnt/<partition> (mount_engine.c's own mount-point convention) —
 * cf_backup_run does not mount it for you; call cf_mount_partition()
 * first. This keeps backup_engine from silently mounting things behind
 * the caller's back.
 */

typedef struct {
    bool boot;
    bool recovery;
    bool system;
    bool vendor;
    bool data;
    bool compress; /* gzip the output (boot/recovery images; system/vendor tars are always gzipped) */
} cf_backup_selection_t;

bool cf_backup_run(const cf_backup_selection_t *selection,
                    const char *fstab_path,
                    const char *dest_dir);

/* Restores a previously-made backup (a timestamped folder cf_backup_run
 * created) back onto the device. Verifies every file's .md5 sidecar
 * before writing anything back — refuses the whole restore if ANY file
 * fails its digest check, rather than partially restoring a backup that
 * might be corrupted. */
bool cf_restore_run(const char *backup_dir, const char *fstab_path);

#endif /* CF_BACKUP_ENGINE_H */
