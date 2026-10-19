#ifndef CF_UI_LOOP_H
#define CF_UI_LOOP_H

#include <stdbool.h>
#include "../gui/theme_xml.h"
#include "../graphics/fb_backend.h"
#include "../graphics/input_backend.h"

/*
 * ui_loop — the piece that was missing: wires data_manager, theme_xml,
 * fb_backend, and input_backend together into an actual render-and-
 * dispatch loop, instead of leaving them as disconnected tested pieces.
 *
 * Theme convention this expects (extends theme_xml.h's own doc example
 * with w/h attributes, needed for hit-testing):
 *   <page name="main">
 *     <object type="button" x="10" y="10" w="80" h="20"
 *             text="Flash" action="flash"/>
 *   </page>
 *
 * TESTABILITY: the event source is a function pointer + context, not a
 * hardcoded call to cf_input_read_event(), so a test can inject a canned
 * sequence of synthetic events (same technique test_fb_text.c and
 * test_input_backend.c already use to avoid needing real hardware)
 * instead of this only being exercisable against a real touchscreen.
 * Similarly, `fb` is a cf_fb_t the caller already has open — a test can
 * hand it an in-memory-buffer-backed one (see test_fb_text.c) rather
 * than this function calling cf_fb_open() itself.
 */

typedef bool (*cf_ui_event_source_fn)(void *ctx, cf_input_event_t *out);

typedef struct {
    const char *action_name;     /* matches a button's "action" XML attribute */
    void (*callback)(void *user_data);
} cf_ui_action_binding_t;

typedef struct {
    cf_fb_t *fb;
    cf_ui_event_source_fn read_event;
    void *event_source_ctx;
    const cf_ui_action_binding_t *bindings;
    int binding_count;
    void *user_data;              /* passed through to every callback */
    uint8_t bg_r, bg_g, bg_b;
    uint8_t button_r, button_g, button_b;
    uint8_t text_r, text_g, text_b;
} cf_ui_loop_config_t;

/*
 * Renders `page`'s button objects to config->fb, then processes events
 * from config->read_event: a TOUCH_UP landing inside a button's
 * (x,y,w,h) rect looks up that button's "action" attribute in
 * config->bindings and invokes the matching callback with
 * config->user_data.
 *
 * Processes at most `max_events` events (or stops early if read_event
 * returns false, signaling no more events) rather than looping forever
 * — real usage (main.c) calls this in its own outer loop or passes a
 * large max_events; tests pass a small bounded count so they terminate
 * deterministically. Returns the number of events actually processed.
 */
int cf_ui_run_page(const cf_ui_loop_config_t *config, const cf_xml_node_t *page, int max_events);

#endif /* CF_UI_LOOP_H */
