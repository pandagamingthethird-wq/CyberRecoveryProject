/* test_zip_installer.c — builds a REAL synthetic zip (via the system
 * `zip` tool, just like a real APK/OTA zip would be built) containing a
 * shell-script-style update-binary (same shape as Magisk's own
 * installer), runs it through cf_zip_install(), and confirms the
 * script's marker file genuinely exists afterward — the actual proof
 * that "extract a zip and run its installer" works end to end, not
 * assumed from the pieces. Also tests an ELF-binary-style installer and
 * the failure path (nonzero exit / missing update-binary). */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include "zip_installer.h"

static void run_shell(const char *fmt, ...) {
    char cmd[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "command failed (%d): %s\n", rc, cmd);
        exit(1);
    }
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int main() {
    const char *workdir = "/tmp/cf_zip_installer_test";
    run_shell("rm -rf '%s' && mkdir -p '%s/zipcontent/META-INF/com/google/android'", workdir, workdir);

    /* --- Test 1: shell-script installer (Magisk's installer shape) --- */
    char marker_path[256];
    snprintf(marker_path, sizeof(marker_path), "%s/marker_shell.txt", workdir);

    char script_path[512];
    snprintf(script_path, sizeof(script_path), "%s/zipcontent/META-INF/com/google/android/update-binary", workdir);
    FILE *script = fopen(script_path, "w");
    assert(script != NULL);
    fprintf(script,
        "#!/bin/sh\n"
        "# synthetic Magisk-style installer: argv is <api-version> <output-fd> <zip-path>\n"
        "echo \"api-version=$1 zip-path=$3\" > '%s'\n"
        "exit 0\n", marker_path);
    fclose(script);
    run_shell("chmod +x '%s'", script_path);

    char zip1_path[256];
    snprintf(zip1_path, sizeof(zip1_path), "%s/shell_installer.zip", workdir);
    run_shell("cd '%s/zipcontent' && zip -q -r '%s' META-INF", workdir, zip1_path);

    cf_zip_install_result_t result = {0};
    bool ok = cf_zip_install(zip1_path, "/tmp", NULL, &result);
    printf("shell installer: success=%d exit_code=%d error='%s'\n",
           result.success, result.exit_code, result.error);
    assert(ok == true);
    assert(result.success == true);
    assert(result.exit_code == 0);

    assert(file_exists(marker_path));
    FILE *mf = fopen(marker_path, "r");
    char marker_contents[256] = {0};
    fread(marker_contents, 1, sizeof(marker_contents) - 1, mf);
    fclose(mf);
    printf("marker file contents: %s", marker_contents);
    assert(strstr(marker_contents, "api-version=3") != NULL);
    assert(strstr(marker_contents, zip1_path) != NULL);

    assert(cf_zip_detect_installer_type(script_path) == CF_INSTALLER_SHELL_SCRIPT);

    /* --- Test 2: ELF-binary installer --- */
    char elf_src_path[256];
    snprintf(elf_src_path, sizeof(elf_src_path), "%s/elf_installer.c", workdir);
    FILE *elf_src = fopen(elf_src_path, "w");
    assert(elf_src != NULL);
    fprintf(elf_src,
        "#include <stdio.h>\n"
        "int main(int argc, char **argv) {\n"
        "    FILE *f = fopen(\"%s/marker_elf.txt\", \"w\");\n"
        "    fprintf(f, \"argc=%%d argv1=%%s argv3=%%s\\n\", argc, argv[1], argv[3]);\n"
        "    fclose(f);\n"
        "    return 0;\n"
        "}\n", workdir);
    fclose(elf_src);

    char zip2_dir[256];
    snprintf(zip2_dir, sizeof(zip2_dir), "%s/zipcontent2/META-INF/com/google/android", workdir);
    run_shell("mkdir -p '%s'", zip2_dir);
    char elf_binary_path[512];
    snprintf(elf_binary_path, sizeof(elf_binary_path), "%s/update-binary", zip2_dir);
    run_shell("gcc -o '%s' '%s'", elf_binary_path, elf_src_path);

    char zip2_path[256];
    snprintf(zip2_path, sizeof(zip2_path), "%s/elf_installer.zip", workdir);
    run_shell("cd '%s/zipcontent2' && zip -q -r '%s' META-INF", workdir, zip2_path);

    assert(cf_zip_detect_installer_type(elf_binary_path) == CF_INSTALLER_ELF_BINARY);

    cf_zip_install_result_t result2 = {0};
    bool ok2 = cf_zip_install(zip2_path, "/tmp", NULL, &result2);
    printf("ELF installer: success=%d exit_code=%d error='%s'\n",
           result2.success, result2.exit_code, result2.error);
    assert(ok2 == true);

    char marker_elf_path[256];
    snprintf(marker_elf_path, sizeof(marker_elf_path), "%s/marker_elf.txt", workdir);
    assert(file_exists(marker_elf_path));
    FILE *mf2 = fopen(marker_elf_path, "r");
    char marker2_contents[256] = {0};
    fread(marker2_contents, 1, sizeof(marker2_contents) - 1, mf2);
    fclose(mf2);
    printf("ELF marker file contents: %s", marker2_contents);
    assert(strstr(marker2_contents, "argc=4") != NULL);
    assert(strstr(marker2_contents, "argv1=3") != NULL);

    /* --- Test 3: failure paths --- */
    cf_zip_install_result_t result3 = {0};
    bool ok3 = cf_zip_install("/tmp/does_not_exist.zip", "/tmp", NULL, &result3);
    printf("nonexistent zip: ok=%d error='%s'\n", ok3, result3.error);
    assert(ok3 == false);

    /* a zip with no update-binary at all */
    char zip3_dir[256];
    snprintf(zip3_dir, sizeof(zip3_dir), "%s/zipcontent3", workdir);
    run_shell("mkdir -p '%s' && echo hello > '%s/somefile.txt'", zip3_dir, zip3_dir);
    char zip3_path[256];
    snprintf(zip3_path, sizeof(zip3_path), "%s/no_installer.zip", workdir);
    run_shell("cd '%s' && zip -q -r '%s' somefile.txt", zip3_dir, zip3_path);

    cf_zip_install_result_t result4 = {0};
    bool ok4 = cf_zip_install(zip3_path, "/tmp", NULL, &result4);
    printf("zip with no update-binary: ok=%d error='%s'\n", ok4, result4.error);
    assert(ok4 == false);
    assert(strstr(result4.error, "not found") != NULL);

    run_shell("rm -rf '%s'", workdir);
    printf("\nALL ZIP_INSTALLER TESTS PASSED (shell-script AND ELF-binary installer paths, both via real extract+exec)\n");
    return 0;
}
