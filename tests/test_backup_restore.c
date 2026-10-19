/* test_backup_restore.c — real backup -> corrupt -> restore round trip
 * against synthetic "partitions" (regular files standing in for block
 * devices — dd works identically on both, so this exercises the real
 * cf_backup_run/cf_restore_run code paths, not a mock). Also confirms
 * restore REFUSES a backup whose .md5 sidecar doesn't match (tampered
 * backup), rather than silently restoring something corrupted. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "backup_engine.h"

static bool files_identical(const char *a, const char *b) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmp -s '%s' '%s'", a, b);
    return system(cmd) == 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <fstab_path> <dest_dir>\n", argv[0]);
        return 2;
    }
    const char *fstab_path = argv[1];
    const char *dest_dir = argv[2];

    /* --- boot/recovery: flat image backup+restore --- */
    cf_backup_selection_t sel = {0};
    sel.boot = true;
    sel.recovery = true;
    sel.compress = false;

    bool backed_up = cf_backup_run(&sel, fstab_path, dest_dir);
    printf("cf_backup_run (boot+recovery): %s\n", backed_up ? "OK" : "FAILED");
    assert(backed_up == true);

    /* Find the timestamped dir cf_backup_run created (it's the only
     * subdirectory of dest_dir). */
    char find_cmd[512];
    snprintf(find_cmd, sizeof(find_cmd), "ls -1 '%s' | tail -1", dest_dir);
    FILE *p = popen(find_cmd, "r");
    char stamp[64] = {0};
    assert(fgets(stamp, sizeof(stamp), p) != NULL);
    pclose(p);
    stamp[strcspn(stamp, "\n")] = '\0';

    char backup_subdir[512];
    snprintf(backup_subdir, sizeof(backup_subdir), "%s/%s", dest_dir, stamp);
    printf("backup landed in: %s\n", backup_subdir);

    char boot_backup[600];
    snprintf(boot_backup, sizeof(boot_backup), "%s/boot.img", backup_subdir);

    /* --- corrupt the LIVE "boot partition" file --- */
    const char *live_boot_path = "/tmp/cf_backup_test_fixtures/boot_partition.img";
    system("dd if=/dev/zero of=/tmp/cf_backup_test_fixtures/boot_partition.img "
           "bs=1 count=100 seek=50 conv=notrunc 2>/dev/null");
    assert(!files_identical(live_boot_path, boot_backup));
    printf("live boot partition corrupted (confirmed differs from backup)\n");

    /* --- restore --- */
    bool restored = cf_restore_run(backup_subdir, fstab_path);
    printf("cf_restore_run: %s\n", restored ? "OK" : "FAILED");
    assert(restored == true);

    assert(files_identical(live_boot_path, boot_backup));
    printf("live boot partition now matches backup after restore — ROUND TRIP CONFIRMED\n");

    /* --- tamper with the backup's .md5 sidecar and confirm restore REFUSES --- */
    char tampered_md5_cmd[700];
    snprintf(tampered_md5_cmd, sizeof(tampered_md5_cmd),
             "echo 'deadbeefdeadbeefdeadbeefdeadbeef  boot.img' > '%s.md5'", boot_backup);
    system(tampered_md5_cmd);

    bool restored_after_tamper = cf_restore_run(backup_subdir, fstab_path);
    printf("cf_restore_run after tampering with .md5 sidecar: %s (must be FAILED)\n",
           restored_after_tamper ? "OK" : "FAILED");
    assert(restored_after_tamper == false);

    /* --- system: file-level (twrp_tar) backup+restore via a synthetic
     * /mnt/system directory --- */
    sleep(1); /* cf_backup_run's stamp dir has 1-second resolution (time(NULL));
                 ensure this backup lands in a genuinely distinct folder from
                 the boot/recovery one above, for a cleaner test. */
    system("mkdir -p /mnt/system/subdir");
    system("echo 'original system content' > /mnt/system/important_file.txt");
    system("echo 'nested' > /mnt/system/subdir/nested_file.txt");

    cf_backup_selection_t sel2 = {0};
    sel2.system = true;
    bool backed_up2 = cf_backup_run(&sel2, fstab_path, dest_dir);
    printf("\ncf_backup_run (system, file-level): %s\n", backed_up2 ? "OK" : "FAILED");
    assert(backed_up2 == true);

    FILE *p2 = popen(find_cmd, "r"); /* latest stamp dir again (new timestamp) */
    char stamp2[64] = {0};
    assert(fgets(stamp2, sizeof(stamp2), p2) != NULL);
    pclose(p2);
    stamp2[strcspn(stamp2, "\n")] = '\0';
    char backup_subdir2[512];
    snprintf(backup_subdir2, sizeof(backup_subdir2), "%s/%s", dest_dir, stamp2);

    /* corrupt the live mounted "system" */
    system("echo 'CORRUPTED' > /mnt/system/important_file.txt");
    system("rm -f /mnt/system/subdir/nested_file.txt");

    bool restored2 = cf_restore_run(backup_subdir2, fstab_path);
    printf("cf_restore_run (system): %s\n", restored2 ? "OK" : "FAILED");
    assert(restored2 == true);

    FILE *check = fopen("/mnt/system/important_file.txt", "r");
    assert(check != NULL);
    char contents[128] = {0};
    fread(contents, 1, sizeof(contents) - 1, check);
    fclose(check);
    printf("restored important_file.txt contents: %s", contents);
    assert(strstr(contents, "original system content") != NULL);

    FILE *check2 = fopen("/mnt/system/subdir/nested_file.txt", "r");
    assert(check2 != NULL); /* must have been recreated by the restore */
    fclose(check2);
    printf("nested file correctly restored too\n");

    printf("\nALL BACKUP_RESTORE ROUND-TRIP TESTS PASSED "
           "(flat dd images AND file-level tar, incl. correct refusal of a "
           "tampered/corrupted backup)\n");
    return 0;
}
