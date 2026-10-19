#include "zip_installer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

static void set_err(cf_zip_install_result_t *result, const char *msg) {
    snprintf(result->error, sizeof(result->error), "%s", msg);
}

cf_installer_type_t cf_zip_detect_installer_type(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return CF_INSTALLER_UNKNOWN;

    unsigned char header[4] = {0};
    size_t n = fread(header, 1, sizeof(header), f);
    fclose(f);
    if (n < 2) return CF_INSTALLER_UNKNOWN;

    if (header[0] == '#' && header[1] == '!') return CF_INSTALLER_SHELL_SCRIPT;
    if (n == 4 && header[0] == 0x7F && header[1] == 'E' && header[2] == 'L' && header[3] == 'F')
        return CF_INSTALLER_ELF_BINARY;

    return CF_INSTALLER_UNKNOWN;
}

/* Extracts the zip via `unzip` (system tool). On-device this is a
 * packaging dependency: a bundled busybox unzip applet, or linking
 * libarchive's zip-reading support directly (same library twrp_tar.c
 * already uses for tar — libarchive also reads zip). Shelling out to
 * `unzip` is what's actually testable/correct right now without that
 * NDK/libarchive-cross-compile work being done.
 *
 * `dest_dir` must already exist (created via mkdtemp by the caller) —
 * this function does NOT mkdir it, since an earlier version used a
 * fixed pid-based directory name that collided across multiple
 * cf_zip_install() calls within the same process, leaking stale files
 * from a previous install into a later one's "extraction" (caught by
 * tests/test_zip_installer.c's "no update-binary" case incorrectly
 * finding a leftover update-binary from an earlier test). */
static bool extract_zip(const char *zip_path, const char *dest_dir, cf_zip_install_result_t *result) {
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "unzip -o -q '%s' -d '%s' 2>&1", zip_path, dest_dir);
    int rc = system(cmd);
    if (rc != 0) {
        set_err(result, "unzip extraction failed");
        return false;
    }
    return true;
}

static bool find_update_binary(const char *extracted_dir, char *out_path, int out_cap) {
    snprintf(out_path, (size_t)out_cap, "%s/META-INF/com/google/android/update-binary", extracted_dir);
    struct stat st;
    return stat(out_path, &st) == 0;
}

bool cf_zip_install(const char *zip_path, const char *work_root,
                     const char *progress_log_path, cf_zip_install_result_t *out) {
    memset(out, 0, sizeof(*out));

    /* mkdtemp gives a genuinely unique directory per call (unlike the
     * previous pid-based scheme, which collided across multiple calls
     * within one process — see extract_zip()'s comment). */
    char template[600];
    int written = snprintf(template, sizeof(template), "%s/cf_install_XXXXXX", work_root);
    if (written < 0 || (size_t)written >= sizeof(template)) {
        set_err(out, "work_root path too long");
        return false;
    }
    if (!mkdtemp(template)) {
        set_err(out, "could not create unique extraction directory (mkdtemp failed)");
        return false;
    }
    snprintf(out->extracted_dir, sizeof(out->extracted_dir), "%s", template);

    if (!extract_zip(zip_path, out->extracted_dir, out)) {
        return false;
    }

    char update_binary[600];
    if (!find_update_binary(out->extracted_dir, update_binary, sizeof(update_binary))) {
        set_err(out, "META-INF/com/google/android/update-binary not found in zip");
        return false;
    }

    chmod(update_binary, 0755);

    cf_installer_type_t type = cf_zip_detect_installer_type(update_binary);
    if (type == CF_INSTALLER_UNKNOWN) {
        set_err(out, "update-binary is neither a shell script nor an ELF binary");
        return false;
    }

    /* Standard Android calling convention: update-binary <api-version>
     * <output-fd> <zip-path>. We use fd 1 (stdout) redirected to
     * progress_log_path rather than a real pipe — simpler, and
     * sufficient for capturing/logging what the installer reports. */
    pid_t pid = fork();
    if (pid < 0) {
        set_err(out, "fork() failed");
        return false;
    }

    if (pid == 0) {
        /* child */
        if (progress_log_path) {
            FILE *log = fopen(progress_log_path, "w");
            if (log) {
                dup2(fileno(log), STDOUT_FILENO);
                fclose(log);
            }
        }
        if (type == CF_INSTALLER_SHELL_SCRIPT) {
            execlp("sh", "sh", update_binary, "3", "1", zip_path, (char *)NULL);
        } else {
            execl(update_binary, update_binary, "3", "1", zip_path, (char *)NULL);
        }
        _exit(127); /* exec failed */
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if (!WIFEXITED(status)) {
        set_err(out, "installer terminated abnormally (signal)");
        out->exit_code = -1;
        return false;
    }

    out->exit_code = WEXITSTATUS(status);
    if (out->exit_code != 0) {
        set_err(out, "installer exited with nonzero status");
        return false;
    }

    out->success = true;
    return true;
}
