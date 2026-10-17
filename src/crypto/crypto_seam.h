#ifndef CF_CRYPTO_SEAM_H
#define CF_CRYPTO_SEAM_H

#include <stdbool.h>

/*
 * crypto_seam — DELIBERATELY UNIMPLEMENTED. This header exists only to
 * document the interface shape a real FBE/FDE decrypt implementation
 * would need, so there's a defined seam for future work rather than
 * nothing at all. Do not add stub bodies that return `true` here — a
 * crypto function that silently no-ops is far more dangerous than one
 * that doesn't exist, since calling code might treat a no-op "success"
 * as "the partition is actually decrypted."
 *
 * Why this is blocked (from the BF7 investigation): /data is f2fs +
 * Trustonic TEE/Keymaster 4.1. Decrypt requires the device's TEE daemon
 * (mcDrvDaemon) to be running and reachable from the recovery ramdisk —
 * confirmed via dmesg that keystore2 crash-loops because mcDrvDaemon
 * never starts, which traces back to the daemon binary/init service
 * being absent from the recovery ramdisk, not to anything on the Android
 * framework/Keymaster-API side. Fixing this means ensuring the TEE
 * daemon + its dependencies (likely vendor-partition binaries) are
 * actually present and launched in the recovery ramdisk's init — THEN
 * this interface can be implemented against the real Keymaster HIDL/AIDL
 * calls.
 *
 * Real TWRP's equivalent lives in crypto/ and talks to Keymaster across
 * several historical versions (0, 1, 2, 3, 4). Once the TEE daemon issue
 * is fixed, implement cf_crypto_unlock() against whichever Keymaster
 * version this device's vendor partition actually exposes — check with
 * `getprop ro.hardware.keystore` / vendor HAL manifest on a booted
 * (non-recovery) instance of the device to find out which.
 */

typedef enum {
    CF_CRYPTO_FBE,  /* File-Based Encryption (per-user keys) */
    CF_CRYPTO_FDE   /* Full-Disk Encryption (whole-partition key) — legacy */
} cf_crypto_mode_t;

typedef struct {
    cf_crypto_mode_t mode;
    char keymaster_version[16]; /* e.g. "4.1" — populate once known */
} cf_crypto_info_t;

/* NOT IMPLEMENTED. Always returns false. Exists only to define the
 * interface a real implementation would fill in once the TEE daemon
 * blocker above is resolved. Do not call this expecting it to work. */
bool cf_crypto_unlock(const char *partition_path, const char *password,
                       cf_crypto_info_t *info_out);

#endif /* CF_CRYPTO_SEAM_H */
