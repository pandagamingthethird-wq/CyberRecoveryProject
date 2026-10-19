/* test_ota_verify_c.c — tests the C port (ota_verify.c) against the
 * SAME 6 adversarial scenarios as tests/test_ota_verify.sh's Python
 * coverage, including the exact "malicious signer" case that caught a
 * real bug in the original implementation. Expects the fixture zips to
 * already exist (built by tests/test_ota_verify_c.sh before running
 * this binary) — see that script for how the fixtures are generated. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ota_verify.h"

static void check(const char *desc, const char *zip_path, const char *cert_path,
                   bool require_chain, bool expect_ok) {
    char err[256] = {0};
    cf_ota_verify_options_t opts = { .require_chain = require_chain, .openssl_path = "openssl" };
    bool ok = cf_ota_verify_zip(zip_path, cert_path, &opts, err, sizeof(err));
    printf("%-55s -> %s%s%s\n", desc, ok ? "OK" : "REJECTED",
           err[0] ? " (" : "", err[0] ? err : "");
    if (err[0]) printf(")\n");
    if (ok != expect_ok) {
        fprintf(stderr, "FAIL: %s — expected %s, got %s\n", desc,
                expect_ok ? "OK" : "REJECTED", ok ? "OK" : "REJECTED");
        exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc != 7) {
        fprintf(stderr, "usage: %s <legit_zip> <malicious_zip> <tampered_zip> <unsigned_zip> <trusted_cert> <unused>\n", argv[0]);
        return 2;
    }
    const char *legit = argv[1];
    const char *malicious = argv[2];
    const char *tampered = argv[3];
    const char *unsigned_zip = argv[4];
    const char *cert = argv[5];

    check("legit zip, correct cert, chain mode",        legit, cert, true, true);
    check("legit zip, correct cert, no-chain mode",      legit, cert, false, true);
    check("MALICIOUS zip (wrong signer), chain mode",    malicious, cert, true, false);
    check("MALICIOUS zip (wrong signer), no-chain mode", malicious, cert, false, false);
    check("tampered content, correct cert",              tampered, cert, true, false);
    check("unsigned zip (no footer at all)",              unsigned_zip, cert, true, false);

    printf("\nALL C OTA VERIFIER TESTS PASSED (matches Python's 6-scenario coverage)\n");
    return 0;
}
