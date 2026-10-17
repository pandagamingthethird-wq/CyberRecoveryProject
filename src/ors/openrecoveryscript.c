#include "openrecoveryscript.h"
#include "../mount/mount_engine.h"
#include "../wipe/wipe_engine.h"
#include "../backup/backup_engine.h"
#include "../gui/data_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *keyword;
    cf_ors_command_type_t type;
    int expected_args; /* 1 or 2 */
} cf_ors_keyword_t;

static const cf_ors_keyword_t KEYWORDS[] = {
    { "mount",   CF_ORS_MOUNT,   1 },
    { "unmount", CF_ORS_UNMOUNT, 1 },
    { "wipe",    CF_ORS_WIPE,    1 },
    { "backup",  CF_ORS_BACKUP,  2 },
    { "restore", CF_ORS_RESTORE, 1 },
    { "install", CF_ORS_INSTALL, 1 },
    { "set",     CF_ORS_SET,     2 },
};
#define KEYWORD_COUNT (int)(sizeof(KEYWORDS) / sizeof(KEYWORDS[0]))

const char *cf_ors_command_name(cf_ors_command_type_t type) {
    for (int i = 0; i < KEYWORD_COUNT; i++) {
        if (KEYWORDS[i].type == type) return KEYWORDS[i].keyword;
    }
    return "unknown";
}

static void trim(char *s) {
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                        s[len-1] == '\r' || s[len-1] == '\n')) {
        s[--len] = '\0';
    }
}

static bool parse_line(const char *raw_line, int line_number, cf_ors_command_t *out) {
    char line[512];
    strncpy(line, raw_line, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    trim(line);

    if (line[0] == '\0' || line[0] == '#') return false;

    char *space = strchr(line, ' ');
    char keyword[32] = {0};
    char rest[480] = {0};

    if (space) {
        int kw_len = (int)(space - line);
        if (kw_len >= (int)sizeof(keyword)) kw_len = sizeof(keyword) - 1;
        memcpy(keyword, line, kw_len);
        keyword[kw_len] = '\0';
        strncpy(rest, space + 1, sizeof(rest) - 1);
        trim(rest);
    } else {
        strncpy(keyword, line, sizeof(keyword) - 1);
    }

    out->source_line = line_number;
    out->type = CF_ORS_UNKNOWN;
    out->arg1[0] = '\0';
    out->arg2[0] = '\0';

    for (int i = 0; i < KEYWORD_COUNT; i++) {
        if (strcmp(keyword, KEYWORDS[i].keyword) == 0) {
            out->type = KEYWORDS[i].type;
            char *rest_space = strchr(rest, ' ');
            if (rest_space && KEYWORDS[i].expected_args >= 2) {
                int a1_len = (int)(rest_space - rest);
                if (a1_len >= CF_ORS_MAX_ARG) a1_len = CF_ORS_MAX_ARG - 1;
                memcpy(out->arg1, rest, a1_len);
                out->arg1[a1_len] = '\0';
                strncpy(out->arg2, rest_space + 1, CF_ORS_MAX_ARG - 1);
                trim(out->arg2);
            } else {
                strncpy(out->arg1, rest, CF_ORS_MAX_ARG - 1);
            }
            return true;
        }
    }

    strncpy(out->arg1, keyword, CF_ORS_MAX_ARG - 1);
    return true;
}

bool cf_ors_parse(const char *script_text, cf_ors_script_t *out) {
    if (!script_text || !out) return false;
    out->count = 0;

    char *copy = strdup(script_text);
    if (!copy) return false;

    char *saveptr = NULL;
    char *line = strtok_r(copy, "\n", &saveptr);
    int line_number = 0;
    while (line && out->count < CF_ORS_MAX_COMMANDS) {
        line_number++;
        cf_ors_command_t cmd;
        if (parse_line(line, line_number, &cmd)) {
            out->commands[out->count++] = cmd;
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }
    free(copy);
    return out->count > 0;
}

/* ---- execution ------------------------------------------------------- */

static bool read_conf_key(const char *conf_path, const char *key, char *out, int out_cap) {
    FILE *f = fopen(conf_path, "r");
    if (!f) return false;

    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s=", key);
    size_t prefix_len = strlen(prefix);

    char line[512];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, prefix, prefix_len) == 0) {
            char *value = line + prefix_len;
            size_t len = strlen(value);
            while (len > 0 && (value[len-1] == '\n' || value[len-1] == '\r')) value[--len] = '\0';
            snprintf(out, out_cap, "%s", value);
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

static bool set_backup_selection_field(cf_backup_selection_t *sel, const char *partition_name) {
    if (strcmp(partition_name, "boot") == 0) { sel->boot = true; return true; }
    if (strcmp(partition_name, "recovery") == 0) { sel->recovery = true; return true; }
    if (strcmp(partition_name, "system") == 0) { sel->system = true; return true; }
    if (strcmp(partition_name, "vendor") == 0) { sel->vendor = true; return true; }
    if (strcmp(partition_name, "data") == 0) { sel->data = true; return true; }
    return false;
}

static bool execute_one(const cf_ors_command_t *cmd, const char *device_conf_path,
                         const char *fstab_path, const char *backup_dest_dir) {
    switch (cmd->type) {
        case CF_ORS_MOUNT:
            return cf_mount_partition(cmd->arg1, device_conf_path);

        case CF_ORS_UNMOUNT:
            return cf_unmount_partition(cmd->arg1);

        case CF_ORS_WIPE:
            return cf_wipe_partition(cmd->arg1, fstab_path);

        case CF_ORS_BACKUP: {
            cf_backup_selection_t sel = {0};
            if (!set_backup_selection_field(&sel, cmd->arg1)) {
                fprintf(stderr, "ors: unknown backup target '%s' (line %d)\n",
                        cmd->arg1, cmd->source_line);
                return false;
            }
            const char *dest = cmd->arg2[0] ? cmd->arg2 : backup_dest_dir;
            return cf_backup_run(&sel, device_conf_path, dest);
        }

        case CF_ORS_RESTORE:
            return cf_restore_run(cmd->arg1, device_conf_path);

        case CF_ORS_INSTALL: {
            char trusted_cert[256];
            if (!read_conf_key(device_conf_path, "OTA_TRUSTED_CERT", trusted_cert, sizeof(trusted_cert))) {
                fprintf(stderr, "ors: install '%s' (line %d) - no OTA_TRUSTED_CERT configured "
                        "in device.conf, cannot verify signature, refusing to proceed\n",
                        cmd->arg1, cmd->source_line);
                return false;
            }
            char verify_cmd[1024];
            snprintf(verify_cmd, sizeof(verify_cmd),
                     "python3 src/ota/verify_ota_zip.py '%s' --cert '%s'",
                     cmd->arg1, trusted_cert);
            int rc = system(verify_cmd);
            if (rc != 0) {
                fprintf(stderr, "ors: install '%s' (line %d) - signature verification "
                        "FAILED, refusing to proceed\n", cmd->arg1, cmd->source_line);
                return false;
            }
            fprintf(stderr, "ors: install '%s' (line %d) - signature verified. "
                    "NOTE: actual flashing/installation is not implemented yet "
                    "(no updater-script/Edify interpreter) - this command only "
                    "confirms the zip is legitimately signed.\n",
                    cmd->arg1, cmd->source_line);
            return true;
        }

        case CF_ORS_SET:
            return cf_dm_set(cmd->arg1, cmd->arg2);

        case CF_ORS_UNKNOWN:
        default:
            fprintf(stderr, "ors: unknown command '%s' at line %d\n",
                    cmd->arg1, cmd->source_line);
            return false;
    }
}

bool cf_ors_execute(const cf_ors_script_t *script, const char *device_conf_path,
                     const char *fstab_path, const char *backup_dest_dir,
                     bool continue_on_error) {
    if (!script) return false;
    cf_dm_init();

    bool all_ok = true;
    for (int i = 0; i < script->count; i++) {
        const cf_ors_command_t *cmd = &script->commands[i];
        bool ok = execute_one(cmd, device_conf_path, fstab_path, backup_dest_dir);
        if (!ok) {
            all_ok = false;
            fprintf(stderr, "ors: command '%s %s' failed at line %d\n",
                    cf_ors_command_name(cmd->type), cmd->arg1, cmd->source_line);
            if (!continue_on_error) return false;
        }
    }
    return all_ok;
}
