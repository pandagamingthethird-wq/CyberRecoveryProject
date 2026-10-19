#ifndef CF_FB_BACKEND_H
#define CF_FB_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

/*
 * fb_backend — a real Linux /dev/fb0 framebuffer backend (same family of
 * API minuitwrp itself uses when it's not on a DRM/KMS device). This
 * replaces ui_theme.c's printf() stubs with actual pixel output.
 *
 * Honest limitation: many modern devices expose DRM/KMS instead of (or in
 * addition to) legacy fbdev, and this backend only handles fbdev. If BF7's
 * recovery kernel only exposes DRM, this needs a DRM/KMS variant instead
 * — check for /dev/fb0 vs /dev/dri/card0 on the actual device before
 * assuming this is the right backend.
 */

typedef struct {
    int fd;
    uint8_t *mem;           /* mmap'd framebuffer memory */
    int width, height;
    int bits_per_pixel;
    int bytes_per_pixel;
    long screen_size_bytes;
    int line_length;        /* bytes per row, may differ from width*bpp due to padding */
} cf_fb_t;

/* Opens /dev/fb0 (or `path` if non-NULL), reads its mode info, and mmaps
 * it. Returns true on success. */
bool cf_fb_open(cf_fb_t *fb, const char *path);

void cf_fb_close(cf_fb_t *fb);

/* Sets a single pixel to an RGB888 color (converted to the framebuffer's
 * actual format). No-ops silently if x/y are out of bounds. */
void cf_fb_set_pixel(cf_fb_t *fb, int x, int y, uint8_t r, uint8_t g, uint8_t b);

/* Fills a rectangle — this is what menu rows/buttons render as until a
 * real font renderer is wired in (see theme_xml.h's own honest-limits
 * note: text rendering itself isn't implemented here). */
void cf_fb_fill_rect(cf_fb_t *fb, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);

void cf_fb_clear(cf_fb_t *fb, uint8_t r, uint8_t g, uint8_t b);

/*
 * cf_fb_draw_text — renders ASCII text using a small built-in 5x7
 * bitmap font (src/graphics/font5x7.c), scaled by `scale` (1 = 5x7
 * pixels per glyph, 2 = 10x14, etc.). Covers space, 0-9, A-Z (both cases
 * render as uppercase — a recovery menu doesn't need case distinction)
 * and basic punctuation ( . , : / - _ ). An unsupported character is
 * rendered as a blank cell rather than skipped, so text stays aligned.
 * Returns the pixel width consumed (useful for right-aligning or
 * centering a follow-up draw call).
 */
int cf_fb_draw_text(cf_fb_t *fb, int x, int y, const char *text, int scale,
                     uint8_t r, uint8_t g, uint8_t b);

#endif /* CF_FB_BACKEND_H */
