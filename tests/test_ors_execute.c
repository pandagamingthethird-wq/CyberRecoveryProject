/* test_ors_execute.c — integration test of cf_ors_execute() driving the
 * REAL mount_engine/wipe_engine/data_manager code, with fake `mount`,
 * `umount`, and `mkfs.ext4` binaries on PATH standing in for the actual
 * system calls (same trick used for wipe_engine's own standalone test),
 * so this verifies the real wiring without needing root or a real block
 * device. See tests/run_tests.sh's comment for how this is invoked.
 *
 * Also specifically regression-tests the Phase 0 fix: mount_engine.c and
 * wipe_engine.c used to resolve the same partition name to two DIFFERENT
 * block device paths (mount via device.conf's PATH_*, wipe via the
 * fstab). Both now read the fstab, so this test parses the fake
 * binaries' captured invocation log and asserts the device path `mount`
 * was given for "system" is byte-identical to the one `mkfs.ext4` (from
 * the "wipe system" step) was given — not just "the script ran". */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "openrecoveryscript.h"
#include "data_manager.h"

/* Finds the "/dev/..." token on the log line containing `marker` (the
 * fake mount/mkfs scripts log "FAKE <cmd> called with: <args...>", and
 * the device path's exact position among the args differs between mount
 * ("-t <fstype> <device> <mountpoint>") and mkfs.ext4 ("-F <device>"),
 * so searching for the "/dev/" prefix rather than a fixed token index is
 * what actually generalizes across both). */
static bool extract_device_path(const char *log_contents, const char *marker, char *out, size_t out_cap) {
    const char *line = strstr(log_contents, marker);
    if (!line) return false;
    const char *dev_start = strstr(line, "/dev/");
    if (!dev_start) return false;
    const char *line_end = strchr(line, '\n');
    if (line_end && dev_start > line_end) return false; /* /dev/ was on a later line, not this one */
    const char *end = strpbrk(dev_start, " \n'\"");
    if (!end) end = dev_start + strlen(dev_start);
    size_t len = (size_t)(end - dev_start);
    if (len >= out_cap) len = out_cap - 1;
    memcpy(out, dev_start, len);
    out[len] = '\0';
    return true;
}

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

    /* Phase 0 regression check: mount and wipe must resolve "system" to
     * the SAME block device. Read the engine-call log the fake
     * mount/mkfs.ext4 binaries wrote during cf_ors_execute() above. */
    const char *log_path = getenv("CF_TEST_LOG");
    if (!log_path) log_path = "/tmp/cf_engine_calls.log";

    FILE *log_f = fopen(log_path, "r");
    assert(log_f != NULL && "engine call log not found — did the fake binaries run?");
    char log_contents[8192] = {0};
    fread(log_contents, 1, sizeof(log_contents) - 1, log_f);
    fclose(log_f);

    char mount_device[256] = {0};
    char wipe_device[256] = {0};
    assert(extract_device_path(log_contents, "FAKE mount called", mount_device, sizeof(mount_device)));
    assert(extract_device_path(log_contents, "FAKE mkfs.ext4 called", wipe_device, sizeof(wipe_device)));

    printf("mount resolved 'system' to: %s\n", mount_device);
    printf("wipe  resolved 'system' to: %s\n", wipe_device);
    assert(strcmp(mount_device, wipe_device) == 0 &&
           "PHASE 0 REGRESSION: mount and wipe resolved 'system' to DIFFERENT block devices");

    printf("ALL ORS_EXECUTE INTEGRATION TESTS PASSED (incl. Phase 0 mount/wipe path-consistency check)\n");
    return 0;
}
