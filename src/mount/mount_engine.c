#include "mount_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Looks up USES_DYNAMIC_PARTITIONS and PATH_<NAME> from device.conf, and
 * for logical partitions runs `dmctl create` (or device-mapper equivalent)
 * before mount, instead of assuming the node already exists. */
bool cf_mount_partition(const char *partition_name, const char *device_conf_path) {
    FILE *f = fopen(device_conf_path, "r");
    if (!f) return false;

    bool is_logical = false;
    char path[256] = {0};
    char key[128];
    snprintf(key, sizeof(key), "PATH_%s=", partition_name);

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "USES_DYNAMIC_PARTITIONS=true", 29) == 0)
            is_logical = true;
        if (strncmp(line, key, strlen(key)) == 0)
            sscanf(line + strlen(key), "%255s", path);
    }
    fclose(f);

    if (path[0] == '\0') {
        fprintf(stderr, "No PATH_%s in device.conf\n", partition_name);
        return false;
    }

    if (is_logical) {
        char dm_cmd[512];
        snprintf(dm_cmd, sizeof(dm_cmd), "dmctl create %s 2>/dev/null", partition_name);
        system(dm_cmd); /* best-effort: node may already exist */
    }

    char mount_point[300];
    snprintf(mount_point, sizeof(mount_point), "/mnt/%s", partition_name);
    char mkdir_cmd[350];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p '%s'", mount_point);
    system(mkdir_cmd);

    char mount_cmd[700];
    snprintf(mount_cmd, sizeof(mount_cmd), "mount '%s' '%s'", path, mount_point);
    return system(mount_cmd) == 0;
}

bool cf_unmount_partition(const char *partition_name) {
    char cmd[300];
    snprintf(cmd, sizeof(cmd), "umount '/mnt/%s' 2>/dev/null", partition_name);
    return system(cmd) == 0;
}
