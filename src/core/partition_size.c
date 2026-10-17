#include "partition_size.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifndef BLKGETSIZE64
#define BLKGETSIZE64 _IOR(0x12, 114, size_t)
#endif

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';
    return s;
}

int cf_load_size_presets(const char *device_conf_path,
                          cf_size_preset_t *out, int max) {
    FILE *f = fopen(device_conf_path, "r");
    if (!f) return 0;

    char sizes_line[512] = {0};
    char labels_line[512] = {0};
    char line[600];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "PARTITION_SIZE_PRESETS=", 23) == 0) {
            snprintf(sizes_line, sizeof(sizes_line), "%s", line + 23);
        } else if (strncmp(line, "PARTITION_SIZE_LABELS=", 22) == 0) {
            snprintf(labels_line, sizeof(labels_line), "%s", line + 22);
        }
    }
    fclose(f);

    int count = 0;
    char *size_tok = strtok(trim(sizes_line), ",");
    char *label_tok = strtok(trim(labels_line), ",");
    while (size_tok && label_tok && count < max) {
        out[count].bytes = atoll(size_tok);
        strncpy(out[count].label, label_tok, sizeof(out[count].label) - 1);
        out[count].label[sizeof(out[count].label) - 1] = '\0';
        count++;
        size_tok = strtok(NULL, ",");
        label_tok = strtok(NULL, ",");
    }
    return count;
}

int64_t cf_get_actual_partition_size(const char *by_name_path) {
    int fd = open(by_name_path, O_RDONLY);
    if (fd < 0) return -1;
    uint64_t size = 0;
    if (ioctl(fd, BLKGETSIZE64, &size) != 0) {
        close(fd);
        return -1;
    }
    close(fd);
    return (int64_t)size;
}
