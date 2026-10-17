#ifndef CF_BACKUP_ENGINE_H
#define CF_BACKUP_ENGINE_H

#include <stdbool.h>

/* Nandroid-style backup/restore — same semantics as TWRP's backup menu:
 * dd/tar each selected partition into a timestamped folder, with optional
 * compression. Partition list comes from device.conf, not hardcoded, so
 * this is reusable across devices. */

typedef struct {
    bool boot;
    bool recovery;
    bool system;
    bool vendor;
    bool data;
    bool compress; /* gzip the output tars */
} cf_backup_selection_t;

bool cf_backup_run(const cf_backup_selection_t *selection,
                    const char *device_conf_path,
                    const char *dest_dir);

bool cf_restore_run(const char *backup_dir, const char *device_conf_path);

#endif /* CF_BACKUP_ENGINE_H */
