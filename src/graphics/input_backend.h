#ifndef CF_INPUT_BACKEND_H
#define CF_INPUT_BACKEND_H

#include <stdbool.h>

/*
 * input_backend — real evdev input reading (the same /dev/input/eventN +
 * struct input_event approach minuitwrp uses), replacing the
 * cf_ui_wait_for_selection() stub that previously always returned 0.
 *
 * Honest limitation: this reads raw events from ONE device node you
 * specify. A real device typically has separate nodes for the touch
 * panel and the volume/power keys, and minuitwrp auto-discovers all
 * input devices by scanning /dev/input/ and checking their reported
 * capabilities (EVIOCGBIT). That auto-discovery isn't implemented here
 * — you currently need to know and pass the right /dev/input/eventN
 * path(s) yourself. Multi-device polling (watching several event nodes
 * at once via epoll/poll) also isn't implemented; this opens one node
 * and blocks on it.
 */

typedef enum {
    CF_INPUT_NONE,
    CF_INPUT_TOUCH_DOWN,
    CF_INPUT_TOUCH_UP,
    CF_INPUT_TOUCH_MOVE,
    CF_INPUT_KEY_VOLUME_UP,
    CF_INPUT_KEY_VOLUME_DOWN,
    CF_INPUT_KEY_POWER,
    CF_INPUT_KEY_OTHER
} cf_input_event_type_t;

typedef struct {
    cf_input_event_type_t type;
    int x, y;       /* valid for touch events */
    int keycode;    /* valid for CF_INPUT_KEY_OTHER */
} cf_input_event_t;

typedef struct {
    int fd;
} cf_input_t;

bool cf_input_open(cf_input_t *in, const char *device_path);
void cf_input_close(cf_input_t *in);

/* Blocks until the next decoded event, or returns false on read error/EOF.
 * Raw multi-touch protocol (ABS_MT_*) coordinate tracking is simplified:
 * this reports the single most recent ABS_X/ABS_Y (or ABS_MT_POSITION_X/Y)
 * pair per SYN_REPORT, which is enough for single-touch recovery UI
 * navigation but not multi-finger gestures. */
bool cf_input_read_event(cf_input_t *in, cf_input_event_t *out);

#endif /* CF_INPUT_BACKEND_H */
