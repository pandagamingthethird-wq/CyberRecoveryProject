#include "fstab_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Splits `line` on whitespace into up to `max_fields` fields, writing
 * each into `fields[i]` (capacity field_cap each). Returns field count. */
static int split_fields(char *line, char fields[][CF_FSTAB_MAX_FIELD],
                         int max_fields, int field_cap) {
    int count = 0;
    char *tok = strtok(line, " \t");
    while (tok && count < max_fields) {
        strncpy(fields[count], tok, field_cap - 1);
        fields[count][field_cap - 1] = '\0';
        count++;
        tok = strtok(NULL, " \t");
    }
    return count;
}

static void parse_one_line(const char *line_in, cf_fstab_t *out) {
    /* Skip comments and blank lines */
    const char *p = line_in;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#' || *p == '\0' || *p == '\n') return;

    char line_copy[512];
    strncpy(line_copy, line_in, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';
    /* strip trailing newline */
    size_t len = strlen(line_copy);
    while (len > 0 && (line_copy[len - 1] == '\n' || line_copy[len - 1] == '\r')) {
        line_copy[--len] = '\0';
    }

    char fields[5][CF_FSTAB_MAX_FIELD];
    int n = split_fields(line_copy, fields, 5, CF_FSTAB_MAX_FIELD);
    if (n < 3) return; /* need at least device, mountpoint, fstype */

    if (out->count >= CF_FSTAB_MAX_ENTRIES) return;
    cf_fstab_entry_t *e = &out->entries[out->count];
    memset(e, 0, sizeof(*e));

    /* Standard order: <device> <mountpoint> <fstype> <mnt_flags> <fsmgr_flags>
     * recovery.fstab historically used <mountpoint> <fstype> <device>
     * instead — detect by checking which field starts with '/dev/' or
     * '/dev/block' (the device column) vs which starts with '/' generally
     * (could be either column in the mountpoint-first variant), falling
     * back to the standard order if ambiguous. */
    bool device_first = (strncmp(fields[0], "/dev/", 5) == 0);

    if (device_first) {
        strncpy(e->block_device, fields[0], CF_FSTAB_MAX_FIELD - 1);
        strncpy(e->mount_point, fields[1], CF_FSTAB_MAX_FIELD - 1);
        strncpy(e->fs_type, fields[2], sizeof(e->fs_type) - 1);
    } else {
        strncpy(e->mount_point, fields[0], CF_FSTAB_MAX_FIELD - 1);
        strncpy(e->fs_type, fields[1], sizeof(e->fs_type) - 1);
        strncpy(e->block_device, fields[2], CF_FSTAB_MAX_FIELD - 1);
    }

    /* Remaining fields (mnt_flags + fsmgr_flags) concatenated into
     * raw_flags for flag scanning and for callers who need the full text. */
    e->raw_flags[0] = '\0';
    for (int i = 3; i < n; i++) {
        strncat(e->raw_flags, fields[i], CF_FSTAB_MAX_FIELD - strlen(e->raw_flags) - 2);
        strncat(e->raw_flags, " ", CF_FSTAB_MAX_FIELD - strlen(e->raw_flags) - 1);
    }

    e->is_logical = (strstr(e->raw_flags, "logical") != NULL);
    e->is_encryptable = (strstr(e->raw_flags, "encryptable=") != NULL ||
                          strstr(e->raw_flags, "fileencryption=") != NULL);

    out->count++;
}

bool cf_fstab_parse_string(const char *content, cf_fstab_t *out) {
    if (!content || !out) return false;
    out->count = 0;

    char *copy = strdup(content);
    if (!copy) return false;

    char *saveptr = NULL;
    char *line = strtok_r(copy, "\n", &saveptr);
    while (line) {
        parse_one_line(line, out);
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(copy);
    return out->count > 0;
}

bool cf_fstab_parse_file(const char *path, cf_fstab_t *out) {
    if (!path || !out) return false;
    out->count = 0;

    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        parse_one_line(line, out);
    }
    fclose(f);
    return out->count > 0;
}

const cf_fstab_entry_t *cf_fstab_find(const cf_fstab_t *fstab, const char *mount_point) {
    if (!fstab || !mount_point) return NULL;
    for (int i = 0; i < fstab->count; i++) {
        if (strcmp(fstab->entries[i].mount_point, mount_point) == 0)
            return &fstab->entries[i];
    }
    return NULL;
}
