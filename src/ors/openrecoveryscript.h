#ifndef CF_ORS_H
#define CF_ORS_H

#include <stdbool.h>

/*
 * openrecoveryscript — a line-based command interpreter matching TWRP's
 * ORS format, for unattended/scripted flash-backup-wipe runs (e.g. from
 * a factory test jig, or chained after a build step).
 *
 * Supported commands (one per line, same vocabulary TWRP's ORS uses):
 *   mount <partition>
 *   unmount <partition>
 *   wipe <partition>
 *   backup <partition> [path]
 *   restore <path>
 *   install <path-to-zip>
 *   set <key> <value>
 *   # comment
 *
 * What's real here: the PARSER (tokenizing a script into commands) is
 * fully implemented and tested standalone. The EXECUTION side
 * (cf_ors_execute) wires each parsed command to the corresponding engine
 * function (mount_engine, backup_engine, flash_engine) — that wiring is
 * straightforward but depends on linking against the rest of the project,
 * so it's not independently testable the way the parser is.
 */

#define CF_ORS_MAX_COMMANDS 64
#define CF_ORS_MAX_ARG 256

typedef enum {
    CF_ORS_MOUNT,
    CF_ORS_UNMOUNT,
    CF_ORS_WIPE,
    CF_ORS_BACKUP,
    CF_ORS_RESTORE,
    CF_ORS_INSTALL,
    CF_ORS_SET,
    CF_ORS_UNKNOWN
} cf_ors_command_type_t;

typedef struct {
    cf_ors_command_type_t type;
    char arg1[CF_ORS_MAX_ARG];
    char arg2[CF_ORS_MAX_ARG]; /* empty if not provided */
    int source_line;           /* 1-based line number, for error reporting */
} cf_ors_command_t;

typedef struct {
    cf_ors_command_t commands[CF_ORS_MAX_COMMANDS];
    int count;
} cf_ors_script_t;

/* Parses an ORS script (as a string, one command per line) into `out`.
 * Unrecognized commands are recorded as CF_ORS_UNKNOWN (not fatal —
 * execution can decide whether to skip or abort on those) rather than
 * failing the whole parse, so one bad line in a long script doesn't
 * lose everything that came before/after it. Blank lines and lines
 * starting with '#' are skipped. Returns true if at least one command
 * was parsed. */
bool cf_ors_parse(const char *script_text, cf_ors_script_t *out);

/* Returns a human-readable name for a command type (for logging). */
const char *cf_ors_command_name(cf_ors_command_type_t type);

/*
 * cf_ors_execute — runs a parsed script against the real engines
 * (mount_engine, wipe_engine, backup_engine; flash_engine for raw image
 * install only — see the INSTALL case's own note below).
 *
 * `device_conf_path` and `fstab_path` are passed through to whichever
 * engine a command needs (mount/wipe use the fstab for auto-discovery;
 * backup/restore use device.conf). `backup_dest_dir` is where `backup`
 * commands without an explicit path write to.
 *
 * Stops at the first command that fails UNLESS `continue_on_error` is
 * true, matching TWRP's own ORS behavior of normally aborting a script
 * on the first failure (since later commands often depend on earlier
 * ones succeeding — e.g. "install" after "wipe cache" assumes cache was
 * actually wiped).
 *
 * IMPORTANT — the INSTALL command only verifies a zip's signature right
 * now (via verify_ota_zip.py); it does NOT parse updater-script/Edify
 * and actually install anything, because that installer doesn't exist
 * yet (see README's "still missing" list). Treat `install` in a script
 * as "check this zip is legitimately signed," not "flash it."
 *
 * Returns true if every command succeeded (or, with continue_on_error,
 * if the script finished running regardless of per-command results).
 */
bool cf_ors_execute(const cf_ors_script_t *script, const char *device_conf_path,
                     const char *fstab_path, const char *backup_dest_dir,
                     bool continue_on_error);

#endif /* CF_ORS_H */
