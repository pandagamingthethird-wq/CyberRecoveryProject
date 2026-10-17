#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "twrp_digest.h"

static void check(const char *input, const char *expected_hex) {
    cf_md5_t d = cf_md5_buffer((const uint8_t *)input, strlen(input));
    char hex[33];
    cf_md5_to_hex(&d, hex);
    printf("md5(\"%s\") = %s  (expected %s)  %s\n",
           input, hex, expected_hex, strcmp(hex, expected_hex) == 0 ? "OK" : "MISMATCH");
    assert(strcmp(hex, expected_hex) == 0);
}

int main() {
    /* Official RFC 1321 test vectors */
    check("", "d41d8cd98f00b204e9800998ecf8427e");
    check("a", "0cc175b9c0f1b6a831c399e269772661");
    check("abc", "900150983cd24fb0d6963f7d28e17f72");
    check("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
    check("abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b");

    /* File-based + sidecar round trip */
    FILE *f = fopen("/tmp/test_backup_file.bin", "wb");
    for (int i = 0; i < 100000; i++) fputc(i % 256, f);
    fclose(f);

    assert(cf_digest_write_sidecar("/tmp/test_backup_file.bin"));
    assert(cf_digest_verify_sidecar("/tmp/test_backup_file.bin"));
    printf("Sidecar write+verify round trip: OK\n");

    /* Corrupt the file, verify should now fail */
    f = fopen("/tmp/test_backup_file.bin", "r+b");
    fseek(f, 50, SEEK_SET);
    fputc(0xFF, f);
    fclose(f);
    assert(!cf_digest_verify_sidecar("/tmp/test_backup_file.bin"));
    printf("Corruption correctly detected: OK\n");

    printf("ALL MD5/DIGEST TESTS PASSED (against official RFC 1321 vectors)\n");
    return 0;
}
