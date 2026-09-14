#ifndef _FONT_H_
#define _FONT_H_

#define FONT_WIDTH        12
#define FONT_HEIGHT       20
#define FONT_GLYPH_BYTES  40   // 12x20 px, rows padded to whole bytes: 2 B * 20
#define FONT_GLYPHS       256

// Defined once in font-data.c (a static const in a header would be emitted
// by every translation unit that indexes it).
extern const unsigned char font[FONT_GLYPHS * FONT_GLYPH_BYTES];

#endif
