#include "input_backend.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

bool cf_input_open(cf_input_t *in, const char *device_path) {
    in->fd = open(device_path, O_RDONLY);
    if (in->fd < 0) {
        perror("cf_input_open");
        return false;
    }
    return true;
}

void cf_input_close(cf_input_t *in) {
    if (in->fd >= 0) close(in->fd);
    in->fd = -1;
}

static cf_input_event_type_t decode_key(int code, bool down) {
    if (!down) return CF_INPUT_NONE; /* only act on key-down, matching TWRP's behavior */
    switch (code) {
        case KEY_VOLUMEUP:   return CF_INPUT_KEY_VOLUME_UP;
        case KEY_VOLUMEDOWN: return CF_INPUT_KEY_VOLUME_DOWN;
        case KEY_POWER:      return CF_INPUT_KEY_POWER;
        default:             return CF_INPUT_KEY_OTHER;
    }
}

bool cf_input_read_event(cf_input_t *in, cf_input_event_t *out) {
    struct input_event ev;
    static int pending_x = -1, pending_y = -1;
    static bool touching = false;
    static bool was_touching = false;

    while (1) {
        ssize_t n = read(in->fd, &ev, sizeof(ev));
        if (n != (ssize_t)sizeof(ev)) return false; /* EOF or error */

        switch (ev.type) {
            case EV_ABS:
                if (ev.code == ABS_X || ev.code == ABS_MT_POSITION_X) pending_x = ev.value;
                else if (ev.code == ABS_Y || ev.code == ABS_MT_POSITION_Y) pending_y = ev.value;
                break;

            case EV_KEY:
                if (ev.code == BTN_TOUCH) {
                    touching = (ev.value != 0);
                    /* report on next SYN so x/y are current */
                    break;
                }
                {
                    cf_input_event_type_t t = decode_key(ev.code, ev.value != 0);
                    if (t != CF_INPUT_NONE) {
                        out->type = t;
                        out->keycode = ev.code;
                        out->x = out->y = 0;
                        return true;
                    }
                }
                break;

            case EV_SYN:
                if (ev.code == SYN_REPORT && pending_x >= 0 && pending_y >= 0) {
                    if (touching && !was_touching) out->type = CF_INPUT_TOUCH_DOWN;
                    else if (!touching && was_touching) out->type = CF_INPUT_TOUCH_UP;
                    else if (touching) out->type = CF_INPUT_TOUCH_MOVE;
                    else break; /* not touching and wasn't — nothing to report */

                    out->x = pending_x;
                    out->y = pending_y;
                    out->keycode = 0;
                    was_touching = touching;
                    return true;
                }
                break;

            default:
                break;
        }
    }
}
