#include <stdio.h>
#include <assert.h>
#include "wipe_engine.h"

int main() {
    assert(cf_wipe_partition("cache", "tests/fixtures/fstab") == true);
    assert(cf_wipe_partition("doesnotexist", "tests/fixtures/fstab") == false);

    printf("ALL WIPE_ENGINE TESTS PASSED\n");
    return 0;
}
