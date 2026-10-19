/* test_fb_text.c — tests cf_fb_draw_text() by constructing a cf_fb_t
 * backed by an in-memory buffer (bypassing cf_fb_open, which needs a
 * real /dev/fb0) and checking specific pixels against the known glyph
 * bitmaps in font5x7.c. No real framebuffer device needed. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "fb_backend.h"

#define FB_W 40
#define FB_H 20

static int pixel_set(cf_fb_t *fb, int x, int y) {
    uint8_t *row = fb->mem + (long)y * fb->line_length;
    uint16_t *px = (uint16_t *)(row + (long)x * 2);
    return *px != 0;
}

int main() {
    cf_fb_t fb = {0};
    fb.width = FB_W;
    fb.height = FB_H;
    fb.bits_per_pixel = 16;
    fb.bytes_per_pixel = 2;
    fb.line_length = FB_W * 2;
    fb.screen_size_bytes = (long)fb.line_length * FB_H;
    fb.mem = calloc(1, (size_t)fb.screen_size_bytes);
    assert(fb.mem != NULL);

    int width = cf_fb_draw_text(&fb, 0, 0, "LI", 1, 255, 255, 255);
    printf("drawn text width: %d px\n", width);
    assert(width == 12); /* 2 glyphs * (5+1) px advance */

    /* 'L' glyph row 0 = 0x10 -> only leftmost column (x=0) set */
    assert(pixel_set(&fb, 0, 0) == 1);
    assert(pixel_set(&fb, 1, 0) == 0);
    assert(pixel_set(&fb, 2, 0) == 0);
    assert(pixel_set(&fb, 3, 0) == 0);
    assert(pixel_set(&fb, 4, 0) == 0);

    /* 'L' glyph row 6 (bottom) = 0x1F -> all 5 columns set */
    for (int x = 0; x < 5; x++) assert(pixel_set(&fb, x, 6) == 1);

    /* 'I' starts at x=6 (glyph advance 6). row 0 = 0x0E -> cols 1,2,3 set
     * (i.e. x=7,8,9), cols 0,4 (x=6,10) clear */
    assert(pixel_set(&fb, 6, 0) == 0);
    assert(pixel_set(&fb, 7, 0) == 1);
    assert(pixel_set(&fb, 8, 0) == 1);
    assert(pixel_set(&fb, 9, 0) == 1);
    assert(pixel_set(&fb, 10, 0) == 0);

    /* A pixel well outside any glyph should be untouched */
    assert(pixel_set(&fb, 20, 15) == 0);

    /* scale=2: 'L' at (0,0), row0 should now occupy a 2x2 block at (0,0) */
    memset(fb.mem, 0, (size_t)fb.screen_size_bytes);
    cf_fb_draw_text(&fb, 0, 0, "L", 2, 255, 255, 255);
    assert(pixel_set(&fb, 0, 0) == 1);
    assert(pixel_set(&fb, 1, 0) == 1);
    assert(pixel_set(&fb, 0, 1) == 1);
    assert(pixel_set(&fb, 1, 1) == 1);
    assert(pixel_set(&fb, 2, 0) == 0); /* next column (col 1 of glyph) is clear for 'L' row0 */

    free(fb.mem);
    printf("ALL FB_TEXT TESTS PASSED\n");
    return 0;
}
