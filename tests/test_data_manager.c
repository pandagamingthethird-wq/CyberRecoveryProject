#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "data_manager.h"

int main() {
    cf_dm_init();
    cf_dm_set("tw_battery", "87");
    cf_dm_set_int("tw_mount_system", 1);

    assert(strcmp(cf_dm_get("tw_battery"), "87") == 0);
    assert(cf_dm_get_int("tw_mount_system", -1) == 1);
    assert(cf_dm_get("nonexistent") == NULL);
    assert(cf_dm_get_int("nonexistent", 42) == 42);

    char out[128];
    assert(cf_dm_substitute("Battery: %tw_battery%%% Mounted: %tw_mount_system%", out, sizeof(out)));
    printf("Substituted: %s\n", out);
    assert(strstr(out, "Battery: 87") != NULL);

    assert(cf_dm_substitute("Unknown var: %nope%", out, sizeof(out)));
    printf("Unknown-var output: %s\n", out);
    assert(strcmp(out, "Unknown var: %nope%") == 0);

    printf("ALL DATA_MANAGER TESTS PASSED\n");
    return 0;
}
