#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "openrecoveryscript.h"

int main() {
    const char *script =
        "# factory test script\n"
        "mount system\n"
        "wipe cache\n"
        "\n"
        "backup boot /sdcard/backups\n"
        "install /sdcard/update.zip\n"
        "set tw_signed_zip_verify 1\n"
        "bogus_command_here foo\n";

    cf_ors_script_t s;
    assert(cf_ors_parse(script, &s));
    printf("Parsed %d commands\n", s.count);
    assert(s.count == 6); /* comment + blank line correctly skipped */

    assert(s.commands[0].type == CF_ORS_MOUNT);
    assert(strcmp(s.commands[0].arg1, "system") == 0);
    printf("[1] %s %s\n", cf_ors_command_name(s.commands[0].type), s.commands[0].arg1);

    assert(s.commands[1].type == CF_ORS_WIPE);
    assert(strcmp(s.commands[1].arg1, "cache") == 0);

    assert(s.commands[2].type == CF_ORS_BACKUP);
    assert(strcmp(s.commands[2].arg1, "boot") == 0);
    assert(strcmp(s.commands[2].arg2, "/sdcard/backups") == 0);
    printf("[3] %s %s %s\n", cf_ors_command_name(s.commands[2].type), s.commands[2].arg1, s.commands[2].arg2);

    assert(s.commands[3].type == CF_ORS_INSTALL);
    assert(strcmp(s.commands[3].arg1, "/sdcard/update.zip") == 0);

    assert(s.commands[4].type == CF_ORS_SET);
    assert(strcmp(s.commands[4].arg1, "tw_signed_zip_verify") == 0);
    assert(strcmp(s.commands[4].arg2, "1") == 0);

    assert(s.commands[5].type == CF_ORS_UNKNOWN);
    assert(strcmp(s.commands[5].arg1, "bogus_command_here") == 0);
    printf("[6] UNKNOWN command flagged correctly: '%s' at line %d\n",
           s.commands[5].arg1, s.commands[5].source_line);

    printf("ALL ORS PARSER TESTS PASSED\n");
    return 0;
}
