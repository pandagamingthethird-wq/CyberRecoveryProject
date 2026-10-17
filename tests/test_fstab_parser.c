#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "fstab_parser.h"

int main() {
    const char *real_bf7_fstab =
        "/system_root       ext4     /dev/block/bootdevice/by-name/system\n"
        "/system_image      ext4     /dev/block/mapper/system          logical\n"
        "/vendor_image      ext4     /dev/block/mapper/vendor          logical\n"
        "/data              f2fs     /dev/block/bootdevice/by-name/userdata   length=-16384\n"
        "# a comment line\n"
        "\n"
        "/cache             ext4     /dev/block/bootdevice/by-name/cache\n";

    cf_fstab_t fstab;
    assert(cf_fstab_parse_string(real_bf7_fstab, &fstab));
    printf("Parsed %d entries\n", fstab.count);
    assert(fstab.count == 5); /* comment + blank line correctly skipped */

    const cf_fstab_entry_t *sys_img = cf_fstab_find(&fstab, "/system_image");
    assert(sys_img != NULL);
    printf("system_image: device=%s fstype=%s logical=%d\n",
           sys_img->block_device, sys_img->fs_type, sys_img->is_logical);
    assert(strcmp(sys_img->block_device, "/dev/block/mapper/system") == 0);
    assert(sys_img->is_logical == true);

    const cf_fstab_entry_t *sys_root = cf_fstab_find(&fstab, "/system_root");
    assert(sys_root != NULL);
    assert(sys_root->is_logical == false);

    const cf_fstab_entry_t *data = cf_fstab_find(&fstab, "/data");
    assert(data != NULL);
    assert(strcmp(data->fs_type, "f2fs") == 0);

    assert(cf_fstab_find(&fstab, "/nonexistent") == NULL);

    printf("ALL FSTAB_PARSER TESTS PASSED (against real BF7 fstab content)\n");
    return 0;
}
