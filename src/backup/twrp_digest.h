#ifndef CF_TWRP_DIGEST_H
#define CF_TWRP_DIGEST_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/*
 * twrp_digest — MD5 checksum generation/verification for backup files,
 * matching TWRP's twrpDigest.cpp role: write a .md5 sidecar next to each
 * backup file, and verify it before a restore so a corrupted backup is
 * caught before it's written back to a partition.
 *
 * This ships its own MD5 implementation (standard RFC 1321 algorithm)
 * rather than depending on OpenSSL's libcrypto — this build environment
 * doesn't reliably have libssl-dev headers available, and a backup
 * integrity checksum doesn't need to be cryptographically
 * collision-resistant (MD5's known weaknesses matter for signatures
 * verifying an adversarial file, not for catching accidental corruption
 * in your own backup — see zip_verify.py for where real signature
 * verification against a real cert actually matters).
 */

typedef struct {
    uint8_t digest[16];
} cf_md5_t;

/* Computes MD5 over `data` of `len` bytes. */
cf_md5_t cf_md5_buffer(const uint8_t *data, size_t len);

/* Computes MD5 over the contents of the file at `path`. Returns false on
 * I/O error (digest contents are undefined in that case). */
bool cf_md5_file(const char *path, cf_md5_t *out);

/* Formats a digest as a lowercase hex string into `out` (must be at
 * least 33 bytes: 32 hex chars + null terminator). */
void cf_md5_to_hex(const cf_md5_t *digest, char *out);

/* Writes a TWRP-style ".md5" sidecar file next to `backup_file_path`
 * (path + ".md5"), containing "<hex digest>  <filename>\n" — same format
 * `md5sum` itself produces, so standard tools can verify it too. */
bool cf_digest_write_sidecar(const char *backup_file_path);

/* Recomputes the digest of `backup_file_path` and compares it against
 * the sidecar ".md5" file. Returns true only if the sidecar exists AND
 * matches. */
bool cf_digest_verify_sidecar(const char *backup_file_path);

#endif /* CF_TWRP_DIGEST_H */
