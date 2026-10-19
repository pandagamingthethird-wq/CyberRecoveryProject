#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "sideload_bridge.h"

void *writer_thread(void *arg) {
    (void)arg;
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000L }; /* 100ms */
    nanosleep(&ts, NULL); /* let the reader open the FIFO first */
    FILE *f = fopen("/tmp/test_sideload.fifo", "wb");
    fwrite("fake OTA zip content, pretend this is 50MB\n", 1, 44, f);
    fclose(f);
    return NULL;
}

int main() {
    unlink("/tmp/test_sideload.fifo");
    assert(cf_sideload_prepare_fifo("/tmp/test_sideload.fifo"));
    assert(cf_sideload_prepare_fifo("/tmp/test_sideload.fifo")); /* idempotent */

    pthread_t t;
    pthread_create(&t, NULL, writer_thread, NULL);

    long received = cf_sideload_receive("/tmp/test_sideload.fifo", "/tmp/test_sideload_output.bin");
    pthread_join(t, NULL);

    printf("Received %ld bytes\n", received);
    assert(received == 44);

    FILE *check = fopen("/tmp/test_sideload_output.bin", "rb");
    char buf[64] = {0};
    fread(buf, 1, 44, check);
    fclose(check);
    printf("Content: %s", buf);
    assert(received > 0);

    printf("ALL SIDELOAD_BRIDGE TESTS PASSED\n");
    return 0;
}
