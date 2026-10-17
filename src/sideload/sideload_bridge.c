#include "sideload_bridge.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

bool cf_sideload_prepare_fifo(const char *fifo_path) {
    struct stat st;
    if (stat(fifo_path, &st) == 0) {
        return S_ISFIFO(st.st_mode); /* exists — fine only if it's really a FIFO */
    }
    if (mkfifo(fifo_path, 0600) != 0) {
        perror("cf_sideload_prepare_fifo: mkfifo");
        return false;
    }
    return true;
}

long cf_sideload_receive(const char *fifo_path, const char *output_path) {
    int in_fd = open(fifo_path, O_RDONLY);
    if (in_fd < 0) {
        perror("cf_sideload_receive: open fifo");
        return -1;
    }

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        perror("cf_sideload_receive: fopen output");
        close(in_fd);
        return -1;
    }

    char buf[65536];
    long total = 0;
    ssize_t n;
    while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
        size_t written = fwrite(buf, 1, (size_t)n, out);
        if (written != (size_t)n) {
            fprintf(stderr, "cf_sideload_receive: short write (disk full?)\n");
            fclose(out);
            close(in_fd);
            return -1;
        }
        total += n;
    }

    fclose(out);
    close(in_fd);

    if (n < 0) {
        perror("cf_sideload_receive: read");
        return -1;
    }
    return total;
}
