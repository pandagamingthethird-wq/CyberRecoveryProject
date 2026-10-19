/* test_e2e_install.c — the capstone test: parses and executes a real ORS
 * script containing "install <zip>", against a REAL signed synthetic
 * zip, through the REAL signature verifier (C port) and REAL zip
 * installer, and confirms the installer's marker file actually exists
 * afterward. This is the test that actually proves "the install
 * mechanism works end to end" rather than assuming it from Phase 0-2's
 * individually-tested pieces. Also confirms a maliciously-signed zip is
 * correctly refused by the same script path.
 *
 * Fixtures (signed zips, certs, device.conf) are built by
 * tests/test_e2e_install.sh before this binary runs — see that script. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "openrecoveryscript.h"

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <device.conf> <fstab> <legit_zip> <malicious_zip>\n", argv[0]);
        return 2;
    }
    const char *device_conf = argv[1];
    const char *fstab = argv[2];
    const char *legit_zip = argv[3];
    const char *malicious_zip = argv[4];

    /* --- Legit, correctly-signed zip: script should succeed --- */
    {
        char script[512];
        snprintf(script, sizeof(script), "install %s\n", legit_zip);

        cf_ors_script_t s;
        assert(cf_ors_parse(script, &s));
        assert(s.count == 1);

        bool ok = cf_ors_execute(&s, device_conf, fstab, "/tmp/test_backups", false);
        printf("legit zip install script: ors_execute returned %s\n", ok ? "true" : "false");
        assert(ok == true);
    }

    assert(file_exists("/tmp/cf_e2e_install_marker.txt"));
    FILE *mf = fopen("/tmp/cf_e2e_install_marker.txt", "r");
    char marker[256] = {0};
    fread(marker, 1, sizeof(marker) - 1, mf);
    fclose(mf);
    printf("marker file contents: %s", marker);
    assert(strstr(marker, "installed-by-e2e-test") != NULL);

    /* --- Malicious (wrong-signer) zip: script must FAIL and must NOT
     * reach the installer at all (no new marker write) --- */
    unlink("/tmp/cf_e2e_install_marker.txt");
    {
        char script[512];
        snprintf(script, sizeof(script), "install %s\n", malicious_zip);

        cf_ors_script_t s;
        assert(cf_ors_parse(script, &s));

        bool ok = cf_ors_execute(&s, device_conf, fstab, "/tmp/test_backups", false);
        printf("malicious zip install script: ors_execute returned %s\n", ok ? "true" : "false");
        assert(ok == false); /* must be rejected */
    }
    assert(!file_exists("/tmp/cf_e2e_install_marker.txt")); /* installer must never have run */

    printf("\nALL END-TO-END INSTALL TESTS PASSED "
           "(ORS script -> C signature verifier -> zip extract+exec, "
           "including correct rejection of a malicious signer)\n");
    return 0;
}
