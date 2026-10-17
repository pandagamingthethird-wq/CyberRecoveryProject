#include "fb_backend.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

bool cf_fb_open(cf_fb_t *fb, const char *path) {
    memset(fb, 0, sizeof(*fb));
    const char *dev_path = path ? path : "/dev/fb0";

    fb->fd = open(dev_path, O_RDWR);
    if (fb->fd < 0) {
        perror("cf_fb_open: open");
        return false;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &vinfo) != 0) {
        perror("cf_fb_open: FBIOGET_VSCREENINFO");
        close(fb->fd);
        return false;
    }
    if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &finfo) != 0) {
        perror("cf_fb_open: FBIOGET_FSCREENINFO");
        close(fb->fd);
        return false;
    }

    fb->width = (int)vinfo.xres;
    fb->height = (int)vinfo.yres;
    fb->bits_per_pixel = (int)vinfo.bits_per_pixel;
    fb->bytes_per_pixel = fb->bits_per_pixel / 8;
    fb->line_length = (int)finfo.line_length;
    fb->screen_size_bytes = (long)finfo.smem_len;

    if (fb->bytes_per_pixel != 2 && fb->bytes_per_pixel != 4) {
        fprintf(stderr, "cf_fb_open: unsupported bpp=%d (only 16/32-bit handled)\n",
                fb->bits_per_pixel);
        close(fb->fd);
        return false;
    }

    fb->mem = mmap(NULL, (size_t)fb->screen_size_bytes, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fb->fd, 0);
    if (fb->mem == MAP_FAILED) {
        perror("cf_fb_open: mmap");
        close(fb->fd);
        fb->mem = NULL;
        return false;
    }

    return true;
}

void cf_fb_close(cf_fb_t *fb) {
    if (fb->mem) munmap(fb->mem, (size_t)fb->screen_size_bytes);
    if (fb->fd >= 0) close(fb->fd);
    memset(fb, 0, sizeof(*fb));
}

static uint32_t pack_color(const cf_fb_t *fb, uint8_t r, uint8_t g, uint8_t b) {
    if (fb->bytes_per_pixel == 2) {
        /* RGB565 — the common case for small-panel recovery framebuffers */
        uint16_t r5 = (r >> 3) & 0x1F;
        uint16_t g6 = (g >> 2) & 0x3F;
        uint16_t b5 = (b >> 3) & 0x1F;
        return (uint32_t)((r5 << 11) | (g6 << 5) | b5);
    }
    /* 32-bit: assume XRGB8888 — if the device actually uses ABGR or
     * similar, this needs the channel order adjusted to match the real
     * vinfo.{red,green,blue}.offset values rather than assuming XRGB. */
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void cf_fb_set_pixel(cf_fb_t *fb, int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!fb->mem || x < 0 || y < 0 || x >= fb->width || y >= fb->height) return;

    uint8_t *row = fb->mem + (long)y * fb->line_length;
    uint32_t color = pack_color(fb, r, g, b);

    if (fb->bytes_per_pixel == 2) {
        uint16_t *px = (uint16_t *)(row + (long)x * 2);
        *px = (uint16_t)color;
    } else {
        uint32_t *px = (uint32_t *)(row + (long)x * 4);
        *px = color;
    }
}

void cf_fb_fill_rect(cf_fb_t *fb, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            cf_fb_set_pixel(fb, xx, yy, r, g, b);
        }
    }
}

void cf_fb_clear(cf_fb_t *fb, uint8_t r, uint8_t g, uint8_t b) {
    cf_fb_fill_rect(fb, 0, 0, fb->width, fb->height, r, g, b);
}
