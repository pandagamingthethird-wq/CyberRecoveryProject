#ifndef CF_OTA_VERIFY_H
#define CF_OTA_VERIFY_H

#include <stdbool.h>

/*
 * ota_verify — C port of verify_ota_zip.py, callable from the on-device
 * binary instead of only host-side via Python. Same algorithm, same
 * security properties, ported deliberately 1:1 rather than
 * reimplemented from scratch, specifically to avoid reintroducing the
 * real bug the Python version had and fixed (see below) — porting logic
 * that already survived adversarial testing is safer than writing new
 * logic that hasn't.
 *
 * Approach: shells out to a bundled/system `openssl` binary (same
 * "bundle a static tool in the ramdisk and exec it" pattern as the
 * planned busybox dependency for zip_installer.c) rather than linking
 * libcrypto directly — this was explicitly the faster, more testable
 * path (see the project's own notes on why: cross-compiling OpenSSL via
 * the NDK is real work not yet done; shelling out to a static openssl
 * binary already works and is provably correct against openssl's own
 * verify command). On a real device, `openssl_path` should point at a
 * statically-linked openssl binary bundled into the recovery ramdisk
 * (the same packaging step zip_installer.c's busybox dependency needs);
 * for host-side testing, the system's own `openssl` is used.
 *
 * SECURITY NOTE — do not weaken the fingerprint check below. An earlier
 * version of the Python verifier's "no-chain" testing mode used
 * `openssl smime -verify -noverify`, which checks a signature is
 * internally self-consistent with WHATEVER certificate is embedded in
 * the message, but never checks that certificate is the one you
 * actually trust. This silently accepted a zip signed with a completely
 * different, untrusted key. The fix — pinning the embedded signer
 * cert's SHA-256 fingerprint against the expected trusted cert,
 * unconditionally, in both chain and no-chain modes — is reproduced
 * here exactly. See tests/test_ota_verify_c.sh for the regression test
 * covering this exact scenario in C, mirroring
 * tests/test_ota_verify.sh's Python coverage.
 */

typedef struct {
    bool require_chain;        /* false = signature + fingerprint pin only, no CA chain validation */
    const char *openssl_path;  /* e.g. "openssl" (system PATH) or a bundled static binary's full path */
} cf_ota_verify_options_t;

/*
 * Verifies `zip_path`'s whole-file signature footer against
 * `trusted_cert_pem_path`. Returns true only if: a signature footer is
 * present and well-formed, the embedded signer cert's fingerprint
 * matches the trusted cert exactly, the cryptographic signature is
 * valid over the file's signed-data portion, and (if
 * options->require_chain) the embedded cert chains to the trusted cert
 * as a CA. `error_out` (optional, may be NULL) receives a short
 * human-readable reason on failure, written into a caller-supplied
 * buffer of `error_out_cap` bytes.
 */
bool cf_ota_verify_zip(const char *zip_path, const char *trusted_cert_pem_path,
                        const cf_ota_verify_options_t *options,
                        char *error_out, int error_out_cap);

#endif /* CF_OTA_VERIFY_H */
