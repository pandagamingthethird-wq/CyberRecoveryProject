/* test_ui_loop.c — integration test of cf_ui_run_page() wiring theme_xml
 * + fb_backend + input_backend's event type together for real, using an
 * in-memory framebuffer (no /dev/fb0 needed) and a canned synthetic
 * event sequence (no real touchscreen needed) — same "test the logic
 * without the hardware" pattern as test_fb_text.c and
 * test_input_backend.c. This is the test that actually proves the UI
 * pieces, previously disconnected, now work together end to end. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ui_loop.h"

typedef struct {
    int flash_calls;
    int backup_calls;
} call_counts_t;

static void on_flash(void *user_data) {
    ((call_counts_t *)user_data)->flash_calls++;
}
static void on_backup(void *user_data) {
    ((call_counts_t *)user_data)->backup_calls++;
}

/* Canned event source: replays a fixed sequence, then signals "no more
 * events" (returns false) once exhausted. */
typedef struct {
    cf_input_event_t events[16];
    int count;
    int index;
} canned_source_t;

static bool canned_read(void *ctx, cf_input_event_t *out) {
    canned_source_t *src = (canned_source_t *)ctx;
    if (src->index >= src->count) return false;
    *out = src->events[src->index++];
    return true;
}

static int pixel_set_rgb565(cf_fb_t *fb, int x, int y) {
    uint8_t *row = fb->mem + (long)y * fb->line_length;
    uint16_t *px = (uint16_t *)(row + (long)x * 2);
    return *px != 0;
}

int main() {
    const char *theme_xml =
        "<page name=\"main\">"
        "  <object type=\"button\" x=\"10\" y=\"10\" w=\"40\" h=\"20\" text=\"OK\" action=\"flash\"/>"
        "  <object type=\"button\" x=\"10\" y=\"40\" w=\"40\" h=\"20\" text=\"OK\" action=\"backup\"/>"
        "</page>";

    static cf_xml_node_t pool[16];
    cf_xml_node_t *page = cf_xml_parse(theme_xml, pool, 16);
    assert(page != NULL);
    assert(page->child_count == 2);

    cf_fb_t fb = {0};
    fb.width = 80;
    fb.height = 80;
    fb.bits_per_pixel = 16;
    fb.bytes_per_pixel = 2;
    fb.line_length = fb.width * 2;
    fb.screen_size_bytes = (long)fb.line_length * fb.height;
    fb.mem = calloc(1, (size_t)fb.screen_size_bytes);
    assert(fb.mem != NULL);

    call_counts_t counts = {0, 0};
    cf_ui_action_binding_t bindings[] = {
        { "flash", on_flash },
        { "backup", on_backup },
    };

    canned_source_t src = {0};
    /* tap inside the "flash" button (10..50, 10..30) */
    src.events[src.count++] = (cf_input_event_t){ .type = CF_INPUT_TOUCH_DOWN, .x = 20, .y = 15 };
    src.events[src.count++] = (cf_input_event_t){ .type = CF_INPUT_TOUCH_UP,   .x = 20, .y = 15 };
    /* tap inside the "backup" button (10..50, 40..60) */
    src.events[src.count++] = (cf_input_event_t){ .type = CF_INPUT_TOUCH_UP,   .x = 25, .y = 45 };
    /* tap outside any button — should dispatch nothing, not crash */
    src.events[src.count++] = (cf_input_event_t){ .type = CF_INPUT_TOUCH_UP,   .x = 70, .y = 70 };
    /* tap on "flash" again */
    src.events[src.count++] = (cf_input_event_t){ .type = CF_INPUT_TOUCH_UP,   .x = 15, .y = 25 };

    cf_ui_loop_config_t config = {
        .fb = &fb,
        .read_event = canned_read,
        .event_source_ctx = &src,
        .bindings = bindings,
        .binding_count = 2,
        .user_data = &counts,
        .bg_r = 0, .bg_g = 0, .bg_b = 0,
        .button_r = 200, .button_g = 200, .button_b = 200,
        .text_r = 255, .text_g = 255, .text_b = 255,
    };

    int processed = cf_ui_run_page(&config, page, /*max_events=*/100);
    printf("events processed: %d\n", processed);
    assert(processed == 5); /* all 5 canned events, then source exhausted */

    printf("flash_calls=%d backup_calls=%d\n", counts.flash_calls, counts.backup_calls);
    assert(counts.flash_calls == 2);   /* tapped twice */
    assert(counts.backup_calls == 1);  /* tapped once */
    /* the outside-tap and the TOUCH_DOWN event correctly triggered no callback */

    /* Confirm rendering actually happened: a pixel inside the flash
     * button's rect should be the button color (nonzero in RGB565),
     * a pixel in the untouched background should be 0 (black bg). */
    assert(pixel_set_rgb565(&fb, 15, 15) == 1);  /* inside flash button */
    assert(pixel_set_rgb565(&fb, 70, 5) == 0);   /* background, no button there */

    /* max_events bound: re-run with only 2 events allowed */
    src.index = 0;
    counts.flash_calls = 0;
    counts.backup_calls = 0;
    int limited = cf_ui_run_page(&config, page, /*max_events=*/2);
    assert(limited == 2);
    assert(counts.flash_calls == 1); /* only the first flash tap was processed */
    assert(counts.backup_calls == 0);

    free(fb.mem);
    printf("ALL UI_LOOP INTEGRATION TESTS PASSED\n");
    return 0;
}
