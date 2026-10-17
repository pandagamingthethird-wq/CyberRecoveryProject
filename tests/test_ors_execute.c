/* test_ors_execute.c — integration test of cf_ors_execute() driving the
 * REAL mount_engine/wipe_engine/data_manager code, with fake `mount`,
 * `umount`, and `mkfs.ext4` binaries on PATH standing in for the actual
 * system calls (same trick used for wipe_engine's own standalone test),
 * so this verifies the real wiring without needing root or a real block
 * device. See tests/run_tests.sh's comment for how this is invoked. */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "openrecoveryscript.h"
#include "data_manager.h"

int main() {
    const char *script =
        "mount system\n"
        "set tw_test_var hello\n"
        "wipe system\n"
        "unmount system\n";

    cf_ors_script_t s;
    assert(cf_ors_parse(script, &s));
    assert(s.count == 4);

    bool ok = cf_ors_execute(&s, "tests/fixtures/device.conf", "tests/fixtures/fstab",
                              "/tmp/test_backups", /*continue_on_error=*/false);
    printf("cf_ors_execute returned: %s\n", ok ? "true" : "false");
    assert(ok == true);

    /* confirm the SET command actually reached DataManager */
    const char *val = cf_dm_get("tw_test_var");
    printf("DataManager tw_test_var = %s\n", val ? val : "(null)");
    assert(val != NULL);
    assert(strcmp(val, "hello") == 0);

    printf("ALL ORS_EXECUTE INTEGRATION TESTS PASSED\n");
    return 0;
}
