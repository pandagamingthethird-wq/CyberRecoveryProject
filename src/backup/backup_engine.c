#include "backup_engine.h"
#include "twrp_digest.h"
#include "twrp_tar.h"
#include "../partition/fstab_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

static bool resolve_block_device(const char *partition_name, const char *fstab_path,
                                  char *out, int out_cap) {
    cf_fstab_t fstab;
    if (!cf_fstab_parse_file(fstab_path, &fstab)) return false;

    char with_slash[256];
    snprintf(with_slash, sizeof(with_slash), "/%s", partition_name);
    const cf_fstab_entry_t *entry = cf_fstab_find(&fstab, with_slash);
    if (!entry) entry = cf_fstab_find(&fstab, partition_name);
    if (!entry) return false;

    snprintf(out, (size_t)out_cap, "%s", entry->block_device);
    return true;
}

static bool dd_partition_to_file(const char *by_name_path, const char *out_path, bool compress) {
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

static bool dd_file_to_partition(const char *image_path, const char *by_name_path, bool compressed) {
    char cmd[1024];
    if (compressed) {
        snprintf(cmd, sizeof(cmd), "gzip -dc '%s' | dd of='%s' bs=4M 2>/dev/null",
                 image_path, by_name_path);
    } else {
        snprintf(cmd, sizeof(cmd), "dd if='%s' of='%s' bs=4M 2>/dev/null",
                 image_path, by_name_path);
    }
    return system(cmd) == 0;
}

static bool backup_flat_image(const char *partition_name, const char *fstab_path,
                               const char *stamp_dir, bool compress) {
    char device_path[256];
    if (!resolve_block_device(partition_name, fstab_path, device_path, sizeof(device_path))) {
        fprintf(stderr, "cf_backup_run: '%s' not found in fstab\n", partition_name);
        return false;
    }

    char out[600];
    snprintf(out, sizeof(out), "%s/%s.img", stamp_dir, partition_name);
    if (!dd_partition_to_file(device_path, out, compress)) return false;

    /* digest the file actually on disk (the .gz if compressed, else the raw image) */
    char digested_path[700];
    snprintf(digested_path, sizeof(digested_path), "%s%s", out, compress ? ".gz" : "");
    return cf_digest_write_sidecar(digested_path);
}

static bool backup_filesystem(const char *partition_name, const char *stamp_dir) {
    char mount_point[300];
    snprintf(mount_point, sizeof(mount_point), "/mnt/%s", partition_name);

    struct stat st;
    if (stat(mount_point, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "cf_backup_run: '%s' is not mounted at %s — mount it first "
                "(cf_backup_run does not mount partitions for you)\n", partition_name, mount_point);
        return false;
    }

    char out[600];
    snprintf(out, sizeof(out), "%s/%s.tar.gz", stamp_dir, partition_name);
    if (!cf_tar_create(mount_point, out)) return false;
    return cf_digest_write_sidecar(out);
}

bool cf_backup_run(const cf_backup_selection_t *sel,
                    const char *fstab_path, const char *dest_dir) {
    char stamp_dir[512];
    time_t now = time(NULL);
    snprintf(stamp_dir, sizeof(stamp_dir), "%s/%ld", dest_dir, (long)now);

    char mk[600];
    snprintf(mk, sizeof(mk), "mkdir -p '%s'", stamp_dir);
    if (system(mk) != 0) return false;

    bool ok = true;
    if (sel->boot)     ok &= backup_flat_image("boot", fstab_path, stamp_dir, sel->compress);
    if (sel->recovery) ok &= backup_flat_image("recovery", fstab_path, stamp_dir, sel->compress);
    if (sel->system)   ok &= backup_filesystem("system", stamp_dir);
    if (sel->vendor)   ok &= backup_filesystem("vendor", stamp_dir);
    if (sel->data) {
        fprintf(stderr, "cf_backup_run: 'data' backup not implemented (encrypted — see "
                "crypto_seam.h's own scope note on why decrypt isn't available)\n");
        ok = false;
    }
    return ok;
}

/* Restores one file found in backup_dir, dispatching by extension:
 * .img / .img.gz -> dd back to the matching partition
 * .tar.gz -> extract back into /mnt/<partition> (must already be mounted) */
static bool restore_one_entry(const char *backup_dir, const char *filename, const char *fstab_path) {
    char full_path[700];
    snprintf(full_path, sizeof(full_path), "%s/%s", backup_dir, filename);

    /* verify digest FIRST, refuse to touch anything if it fails */
    if (!cf_digest_verify_sidecar(full_path)) {
        fprintf(stderr, "cf_restore_run: '%s' FAILED digest verification — refusing to restore "
                "(missing .md5 sidecar, or file is corrupted/tampered)\n", filename);
        return false;
    }

    char partition_name[256];
    strncpy(partition_name, filename, sizeof(partition_name) - 1);
    partition_name[sizeof(partition_name) - 1] = '\0';

    char *ext = strstr(partition_name, ".img.gz");
    if (ext) {
        *ext = '\0';
        char device_path[256];
        if (!resolve_block_device(partition_name, fstab_path, device_path, sizeof(device_path))) {
            fprintf(stderr, "cf_restore_run: '%s' not found in fstab\n", partition_name);
            return false;
        }
        return dd_file_to_partition(full_path, device_path, /*compressed=*/true);
    }

    ext = strstr(partition_name, ".img");
    if (ext) {
        *ext = '\0';
        char device_path[256];
        if (!resolve_block_device(partition_name, fstab_path, device_path, sizeof(device_path))) {
            fprintf(stderr, "cf_restore_run: '%s' not found in fstab\n", partition_name);
            return false;
        }
        return dd_file_to_partition(full_path, device_path, /*compressed=*/false);
    }

    ext = strstr(partition_name, ".tar.gz");
    if (ext) {
        *ext = '\0';
        char mount_point[300];
        snprintf(mount_point, sizeof(mount_point), "/mnt/%s", partition_name);
        struct stat st;
        if (stat(mount_point, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "cf_restore_run: '%s' is not mounted at %s — mount it first\n",
                    partition_name, mount_point);
            return false;
        }
        return cf_tar_extract(full_path, mount_point);
    }

    return true; /* unrecognized file (e.g. a stray .md5) — not an error, just skipped */
}

bool cf_restore_run(const char *backup_dir, const char *fstab_path) {
    DIR *d = opendir(backup_dir);
    if (!d) {
        fprintf(stderr, "cf_restore_run: could not open backup dir '%s'\n", backup_dir);
        return false;
    }

    bool ok = true;
    bool found_anything = false;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".md5") == 0) continue; /* sidecar, not a payload */

        found_anything = true;
        if (!restore_one_entry(backup_dir, ent->d_name, fstab_path)) {
            ok = false;
        }
    }
    closedir(d);

    if (!found_anything) {
        fprintf(stderr, "cf_restore_run: no backup files found in '%s'\n", backup_dir);
        return false;
    }
    return ok;
}
