#ifndef CF_DATA_MANAGER_H
#define CF_DATA_MANAGER_H

#include <stdbool.h>

/*
 * DataManager — equivalent to TWRP's data.cpp/variables.cpp: a live
 * key-value store the GUI reads from instead of hardcoding values, so a
 * theme can reference "%tw_battery%" and get the current value without
 * the UI code and the data source being wired together directly.
 *
 * Real/tested: get/set/substitute all work and are covered by a
 * standalone test. What's NOT here yet: persistence across reboots
 * (TWRP keeps some variables in a settings file) and the "const"
 * variable registration TWRP uses to mark some keys read-only from the
 * GUI side — anything can currently be set.
 */

#define CF_DM_MAX_KEY 64
#define CF_DM_MAX_VALUE 256
#define CF_DM_MAX_ENTRIES 256

void cf_dm_init(void);

bool cf_dm_set(const char *key, const char *value);
bool cf_dm_set_int(const char *key, int value);

/* Returns NULL if key doesn't exist. */
const char *cf_dm_get(const char *key);
int cf_dm_get_int(const char *key, int default_value);

/*
 * Substitutes every "%key%" occurrence in `template_str` with
 * cf_dm_get(key), writing the result into `out` (capacity out_cap).
 * Unknown keys are left as literal "%key%" text (matches TWRP's
 * behavior of not silently eating unrecognized template variables).
 * Returns false if `out` was too small to hold the result.
 */
bool cf_dm_substitute(const char *template_str, char *out, int out_cap);

#endif /* CF_DATA_MANAGER_H */
