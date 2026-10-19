#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include "twrp_tar.h"

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int main() {
    system("rm -rf /tmp/tar_test_src /tmp/tar_test_dest /tmp/tar_test.tar.gz");
    system("mkdir -p /tmp/tar_test_src/subdir");
    system("echo 'hello world' > /tmp/tar_test_src/file1.txt");
    system("echo 'nested content' > /tmp/tar_test_src/subdir/file2.txt");

    printf("=== cf_tar_create ===\n");
    bool created = cf_tar_create("/tmp/tar_test_src", "/tmp/tar_test.tar.gz");
    printf("created: %d\n", created);
    assert(created == true);
    assert(file_exists("/tmp/tar_test.tar.gz"));

    printf("=== cf_tar_extract ===\n");
    system("mkdir -p /tmp/tar_test_dest");
    bool extracted = cf_tar_extract("/tmp/tar_test.tar.gz", "/tmp/tar_test_dest");
    printf("extracted: %d\n", extracted);
    assert(extracted == true);

    assert(file_exists("/tmp/tar_test_dest/file1.txt"));
    assert(file_exists("/tmp/tar_test_dest/subdir/file2.txt"));

    FILE *f1 = fopen("/tmp/tar_test_dest/file1.txt", "r");
    char buf1[64] = {0};
    fread(buf1, 1, sizeof(buf1)-1, f1);
    fclose(f1);
    printf("file1.txt contents: %s", buf1);
    assert(strstr(buf1, "hello world") != NULL);

    FILE *f2 = fopen("/tmp/tar_test_dest/subdir/file2.txt", "r");
    char buf2[64] = {0};
    fread(buf2, 1, sizeof(buf2)-1, f2);
    fclose(f2);
    printf("subdir/file2.txt contents: %s", buf2);
    assert(strstr(buf2, "nested content") != NULL);

    printf("\nALL TWRP_TAR ROUND-TRIP TESTS PASSED (against real libarchive.so.13)\n");
    return 0;
}
