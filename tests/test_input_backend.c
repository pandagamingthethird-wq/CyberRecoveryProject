#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <linux/input.h>
#include "input_backend.h"

static void write_ev(int fd, unsigned short type, unsigned short code, int value) {
    struct input_event ev = {0};
    ev.type = type; ev.code = code; ev.value = value;
    write(fd, &ev, sizeof(ev));
}

int main() {
    int pipefd[2];
    pipe(pipefd);

    cf_input_t in;
    in.fd = pipefd[0];

    /* Simulate: touch down at (100,200), then move to (150,250), then up */
    write_ev(pipefd[1], EV_ABS, ABS_X, 100);
    write_ev(pipefd[1], EV_ABS, ABS_Y, 200);
    write_ev(pipefd[1], EV_KEY, BTN_TOUCH, 1);
    write_ev(pipefd[1], EV_SYN, SYN_REPORT, 0);

    cf_input_event_t out;
    assert(cf_input_read_event(&in, &out));
    printf("Event 1: type=%d x=%d y=%d\n", out.type, out.x, out.y);
    assert(out.type == CF_INPUT_TOUCH_DOWN);
    assert(out.x == 100 && out.y == 200);

    write_ev(pipefd[1], EV_ABS, ABS_X, 150);
    write_ev(pipefd[1], EV_ABS, ABS_Y, 250);
    write_ev(pipefd[1], EV_SYN, SYN_REPORT, 0);
    assert(cf_input_read_event(&in, &out));
    printf("Event 2: type=%d x=%d y=%d\n", out.type, out.x, out.y);
    assert(out.type == CF_INPUT_TOUCH_MOVE);
    assert(out.x == 150 && out.y == 250);

    write_ev(pipefd[1], EV_KEY, BTN_TOUCH, 0);
    write_ev(pipefd[1], EV_SYN, SYN_REPORT, 0);
    assert(cf_input_read_event(&in, &out));
    printf("Event 3: type=%d\n", out.type);
    assert(out.type == CF_INPUT_TOUCH_UP);

    /* Volume key */
    write_ev(pipefd[1], EV_KEY, KEY_VOLUMEUP, 1);
    assert(cf_input_read_event(&in, &out));
    printf("Event 4: type=%d\n", out.type);
    assert(out.type == CF_INPUT_KEY_VOLUME_UP);

    printf("ALL INPUT_BACKEND TESTS PASSED\n");
    return 0;
}
