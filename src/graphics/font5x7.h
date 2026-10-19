#ifndef CF_FONT5X7_H
#define CF_FONT5X7_H

#include <stdint.h>

/*
 * font5x7 — a small, original 5x7 bitmap font (not copied from any
 * external font file/library — designed directly for this project to
 * avoid any licensing question around embedded font data). Covers
 * space, 0-9, A-Z (letters render uppercase regardless of input case —
 * a recovery menu doesn't need case distinction) and a handful of
 * punctuation marks a recovery UI actually needs: . , : / - _
 *
 * Each glyph is 7 bytes (one per row, top to bottom). Within a byte,
 * bit 4 is the leftmost of the 5 columns and bit 0 is the rightmost
 * (i.e. bits 4..0 read left-to-right); bits 7..5 are unused/zero.
 */

/* Returns a pointer to the 7-byte glyph for `c`, or the space glyph if
 * `c` isn't covered (so unsupported characters render as blank rather
 * than crashing or misaligning text). */
const uint8_t *cf_font5x7_glyph(char c);

#define CF_FONT5X7_WIDTH 5
#define CF_FONT5X7_HEIGHT 7

#endif /* CF_FONT5X7_H */
