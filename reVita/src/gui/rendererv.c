#include <vitasdkkern.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "img/font.h"
#include "img/icons-font.h"
#include "rendererv.h"
#include "../fio/theme.h"
#include "../common.h"
#include "../log.h"

// Palette layout: slots 0..THEME_COLOR__NUM-1 mirror theme[], then one slot
// for a colour that is not in the theme (no caller does this today) and one
// for opaque black.
#define VFB_IDX_CUSTOM  (THEME_COLOR__NUM)
#define VFB_IDX_BLACK   (THEME_COLOR__NUM + 1)
_Static_assert(THEME_COLOR__NUM + 2 <= VFB_PAL_NUM, "theme palette does not fit in 4 bits per pixel");
_Static_assert(VFB_W % 2 == 0, "VFB width must be even");

static uint8_t  vfb[VFB_H * VFB_STRIDE];	// 272 * 240 = 65280 B, allocated once with the module
static uint32_t pal[VFB_PAL_NUM];
static uint32_t customColor = 0xFF000000;
static uint8_t  colorIdx = VFB_IDX_BLACK;
static bool     isStripped;

uint32_t uiWidth = VFB_W, uiHeight = VFB_H;

static uint8_t colorToIdx(uint32_t c){
	for (int i = 0; i < THEME_COLOR__NUM; i++)
		if (theme[i] == c)
			return (uint8_t)i;
	if (c == 0xFF000000)
		return VFB_IDX_BLACK;
	if (customColor != c)
		LOG("rendererv: non-theme colour %08X\n", (unsigned)c);
	customColor = c;
	return VFB_IDX_CUSTOM;
}

void rendererv_syncPalette(){
	for (int i = 0; i < THEME_COLOR__NUM; i++)
		pal[i] = theme[i];
	pal[VFB_IDX_CUSTOM] = customColor;
	pal[VFB_IDX_BLACK]  = 0xFF000000;
}

// Even x lives in the low nibble, odd x in the high nibble.
// The unsigned compare rejects negative coordinates as well as too-large ones.
static inline void vput(int32_t x, int32_t y, uint8_t idx){
	if ((uint32_t)x >= uiWidth || (uint32_t)y >= uiHeight)
		return;
	uint8_t* b = &vfb[(uint32_t)y * VFB_STRIDE + ((uint32_t)x >> 1)];
	if (x & 1)
		*b = (uint8_t)((*b & 0x0F) | (idx << 4));
	else
		*b = (uint8_t)((*b & 0xF0) | idx);
}

// Fill pixels [x0, x1) of row y; the caller guarantees the range is clipped and non-empty.
static inline void fillRow(uint32_t y, uint32_t x0, uint32_t x1, uint8_t idx){
	uint8_t* row = &vfb[y * VFB_STRIDE];
	if (x0 & 1){
		row[x0 >> 1] = (uint8_t)((row[x0 >> 1] & 0x0F) | (idx << 4));
		x0++;
	}
	if (x1 & 1){
		x1--;
		row[x1 >> 1] = (uint8_t)((row[x1 >> 1] & 0xF0) | idx);
	}
	if (x1 > x0)
		memset(&row[x0 >> 1], idx | (idx << 4), (x1 - x0) >> 1);
}

void rendererv_expandRow(uint32_t y, uint32_t* dst){
	if (y >= uiHeight)
		return;
	const uint8_t* src = &vfb[y * VFB_STRIDE];
	for (uint32_t k = 0; k < uiWidth / 2; k++){
		uint8_t b = src[k];
		dst[2 * k]     = pal[b & 0x0F];
		dst[2 * k + 1] = pal[b >> 4];
	}
}

void rendererv_drawImage(int32_t x, int32_t y, int32_t w, int32_t h, const unsigned char* img){
	uint32_t idx = 0;
	uint8_t bitN = 0;
	for (int32_t j = 0; j < h; j++){
		for (int32_t i = 0; i < w; i++){
			if (bitN >= 8){
				idx++;
				bitN = 0;
			}
			if (READ(img[idx], (7 - bitN)))
				vput(x + i, y + j, colorIdx);
			bitN++;
		}
		if (bitN != 0){	// rows are padded to whole bytes
			idx++;
			bitN = 0;
		}
	}
}

static void drawRectIdx(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t idx){
	if (w <= 0 || h <= 0)
		return;
	int32_t x0 = max(x, 0), y0 = max(y, 0);
	int32_t x1 = min(x + w, (int32_t)uiWidth), y1 = min(y + h, (int32_t)uiHeight);
	if (x0 >= x1 || y0 >= y1)
		return;
	for (int32_t j = y0; j < y1; j++)
		fillRow((uint32_t)j, (uint32_t)x0, (uint32_t)x1, idx);
}

void rendererv_drawRectangle(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t clr){
	drawRectIdx(x, y, w, h, colorToIdx(clr));
}

void rendererv_drawChar(char character, int x, int y){
	unsigned char c = (unsigned char)character;	// every byte value has a glyph
	rendererv_drawImage(x, y, FONT_WIDTH, FONT_HEIGHT, &font[c * FONT_GLYPH_BYTES]);
}

void rendererv_drawCharIcon(char character, int x, int y){
	unsigned char c = (unsigned char)character;
	if (c >= ICON_ID__NUM)
		return;
	rendererv_drawImage(x, y - 1, ICON_W, ICON_H, &ICON[c * ICON_BYTES]);
}

void rendererv_drawString(int x, int y, const char *str){
	size_t len = strlen(str);
	for (size_t i = 0; i < len; i++){
		if (str[i] == ' ') continue;
		if (str[i] == '$'){
			if (i + 1 >= len) break;	// stray '$' at the end of the string
			rendererv_drawCharIcon(str[i + 1], x + i * CHA_W, y);
			i++;
		} else {
			rendererv_drawChar(str[i], x + i * CHA_W, y);
		}
	}
	if (isStripped)
		drawRectIdx(x, y + CHA_H / 2, len * CHA_W, 2, colorIdx);
}

void rendererv_drawStringF(int x, int y, const char *format, ...){
	char str[512] = { 0 };
	va_list va;

	va_start(va, format);
	vsnprintf(str, sizeof(str), format, va);
	va_end(va);

	rendererv_drawString(x, y, str);
}

void rendererv_setColor(uint32_t c){
	colorIdx = colorToIdx(c);
}

void rendererv_setStripped(bool flag){
	isStripped = flag;
}

void rendererv_init(uint32_t w, uint32_t h){
	uiWidth  = min(w, VFB_W);
	uiHeight = min(h, VFB_H);
	memset(vfb, 0, sizeof(vfb));
	rendererv_syncPalette();
}

void rendererv_destroy(){
}
