#include "data_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char key[CF_DM_MAX_KEY];
    char value[CF_DM_MAX_VALUE];
    bool used;
} cf_dm_entry_t;

static cf_dm_entry_t g_entries[CF_DM_MAX_ENTRIES];
static bool g_initialized = false;

void cf_dm_init(void) {
    memset(g_entries, 0, sizeof(g_entries));
    g_initialized = true;
}

static cf_dm_entry_t *find_entry(const char *key) {
    for (int i = 0; i < CF_DM_MAX_ENTRIES; i++) {
        if (g_entries[i].used && strncmp(g_entries[i].key, key, CF_DM_MAX_KEY) == 0)
            return &g_entries[i];
    }
    return NULL;
}

static cf_dm_entry_t *find_free_slot(void) {
    for (int i = 0; i < CF_DM_MAX_ENTRIES; i++) {
        if (!g_entries[i].used) return &g_entries[i];
    }
    return NULL;
}

bool cf_dm_set(const char *key, const char *value) {
    if (!g_initialized) cf_dm_init();
    if (!key || !value) return false;

    cf_dm_entry_t *e = find_entry(key);
    if (!e) {
        e = find_free_slot();
        if (!e) return false; /* store full */
        strncpy(e->key, key, CF_DM_MAX_KEY - 1);
        e->key[CF_DM_MAX_KEY - 1] = '\0';
        e->used = true;
    }
    strncpy(e->value, value, CF_DM_MAX_VALUE - 1);
    e->value[CF_DM_MAX_VALUE - 1] = '\0';
    return true;
}

bool cf_dm_set_int(const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return cf_dm_set(key, buf);
}

const char *cf_dm_get(const char *key) {
    if (!g_initialized) cf_dm_init();
    cf_dm_entry_t *e = find_entry(key);
    return e ? e->value : NULL;
}

int cf_dm_get_int(const char *key, int default_value) {
    const char *v = cf_dm_get(key);
    if (!v) return default_value;
    char *endptr;
    long parsed = strtol(v, &endptr, 10);
    if (endptr == v) return default_value; /* not a number */
    return (int)parsed;
}

bool cf_dm_substitute(const char *template_str, char *out, int out_cap) {
    if (!template_str || !out || out_cap <= 0) return false;

    int out_pos = 0;
    const char *p = template_str;

    while (*p) {
        if (*p == '%') {
            const char *close = strchr(p + 1, '%');
            if (close && close > p + 1) {
                char key[CF_DM_MAX_KEY];
                int key_len = (int)(close - (p + 1));
                if (key_len < CF_DM_MAX_KEY) {
                    memcpy(key, p + 1, key_len);
                    key[key_len] = '\0';
                    const char *value = cf_dm_get(key);
                    if (value) {
                        int value_len = (int)strlen(value);
                        if (out_pos + value_len >= out_cap) return false;
                        memcpy(out + out_pos, value, value_len);
                        out_pos += value_len;
                        p = close + 1;
                        continue;
                    }
                }
            }
        }
        if (out_pos + 1 >= out_cap) return false;
        out[out_pos++] = *p++;
    }
    out[out_pos] = '\0';
    return true;
}
