#ifndef _RENDERERV_H_
#define _RENDERERV_H_

#include <stdint.h>
#include <stdbool.h>

#define CHA_W  12		//Character size in pixels
#define CHA_H  20

// Virtual framebuffer for the menu: fixed size, 4 bits per pixel, palette
// indexed. It lives in the module's own .bss, so opening the menu never has to
// allocate kernel memory (the old 512 KB per-open memblock was the source of
// the "Buy more RAM !" popup on systems with other heavy kernel plugins).
#define VFB_W          480
#define VFB_H          272
#define VFB_BPP        4
#define VFB_STRIDE     (VFB_W * VFB_BPP / 8)	// 240 B per row
#define VFB_PAL_NUM    16

extern uint32_t uiWidth, uiHeight;

void rendererv_drawChar(char character, int x, int y);
void rendererv_drawCharIcon(char character, int x, int y);
void rendererv_drawImage(int32_t x, int32_t y, int32_t w, int32_t h, const unsigned char* img);
void rendererv_drawString(int x, int y, const char *str);
void rendererv_drawStringF(int x, int y, const char *format, ...);
void rendererv_drawRectangle(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t clr);

void rendererv_setColor(uint32_t clr);
void rendererv_setStripped(bool flag);

// Refresh the palette from theme[]; call once per frame before expanding rows
void rendererv_syncPalette();
// Expand one row of the virtual framebuffer to 32-bit ARGB (dst holds uiWidth entries)
void rendererv_expandRow(uint32_t y, uint32_t* dst);

void rendererv_init(uint32_t w, uint32_t h);
void rendererv_destroy();

#endif
