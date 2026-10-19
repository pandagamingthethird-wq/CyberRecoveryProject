#ifndef CF_ZIP_INSTALLER_H
#define CF_ZIP_INSTALLER_H

#include <stdbool.h>

/*
 * zip_installer — this is the piece that was the actual blocker for
 * "can I flash a ROM or Magisk.zip with this": extract a zip, find its
 * installer, execute it with the correct calling convention.
 *
 * Supports two installer shapes, both real Android conventions:
 *   - A shell script at META-INF/com/google/android/update-binary
 *     (Magisk's installer is this shape) — executed through a shell.
 *   - A self-contained ELF binary at the same path (most stock AOSP
 *     update-binary files are this shape, doing their own Edify
 *     interpretation internally) — executed directly.
 * Both are invoked with the standard Android calling convention:
 *     update-binary <api-version> <output-fd> <zip-path>
 * where api-version is always "3" for anything modern, and output-fd is
 * where the installer writes progress lines (this implementation
 * redirects it to a regular file rather than a real pipe, which is
 * simpler and sufficient for capturing/logging progress).
 *
 * HONEST SCOPE: this proves the MECHANISM works — extract, locate,
 * detect type, exec with the right argv, capture the result — tested
 * against a real synthetic shell-script installer end to end (see
 * tests/test_zip_installer.c). It does NOT claim compatibility with
 * real Magisk's actual (much more complex) installer script, which
 * expects things like a working busybox applet set, specific mount
 * points, and environment variables this hasn't been exercised against.
 * Getting a REAL Magisk.zip to install successfully also requires
 * bundling a static busybox/toybox in the recovery ramdisk (not done
 * here — this is a ramdisk-packaging step, separate from this exec
 * logic) since Magisk's script calls busybox applets throughout.
 *
 * This depends on a shell being available on PATH to run shell-script
 * installers (tested here against the host's own /bin/sh; a real
 * on-device deployment needs a bundled static shell in the ramdisk,
 * same packaging note as above).
 */

typedef enum {
    CF_INSTALLER_SHELL_SCRIPT,
    CF_INSTALLER_ELF_BINARY,
    CF_INSTALLER_UNKNOWN
} cf_installer_type_t;

typedef struct {
    bool success;
    int exit_code;
    char extracted_dir[600];   /* where the zip was extracted — caller may want to inspect/clean up */
    char error[256];
} cf_zip_install_result_t;

/* Extracts `zip_path` into a fresh temp directory under `work_root`
 * (e.g. "/tmp" or a ramdisk tmpfs mount), locates
 * META-INF/com/google/android/update-binary, detects whether it's a
 * shell script or ELF binary, and executes it with the standard
 * calling convention. `progress_log_path` is where the installer's
 * stdout/the output-fd gets written (pass NULL to discard it).
 *
 * This does NOT verify the zip's signature — call cf_ota_verify_zip()
 * (ota_verify.h) first and refuse to proceed if it returns false. This
 * function assumes the caller already decided the zip is trusted.
 */
bool cf_zip_install(const char *zip_path, const char *work_root,
                     const char *progress_log_path, cf_zip_install_result_t *result);

/* Exposed for testing/inspection: detects installer type by reading the
 * first few bytes of the file at `path` ("#!" = shell script, the ELF
 * magic 0x7F 'E' 'L' 'F' = ELF binary). */
cf_installer_type_t cf_zip_detect_installer_type(const char *path);

#endif /* CF_ZIP_INSTALLER_H */
