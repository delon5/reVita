#include <vitasdkkern.h>
#include <psp2kern/display.h> 
#include <psp2kern/kernel/sysmem.h> 
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "img/font.h"
#include "img/icons-font.h"
#include "renderer.h"
#include "rendererv.h"
#include "../common.h"
#include "../log.h"

#define BLACK 0xFF000000

static uint32_t color;
static bool isStripped;

uint32_t* fb_base;
uint32_t fbWidth, fbHeight, fbPitch;

// Scratch row for kernel->user copies. Every user of the renderer runs under
// mutex_gui_uid (gui_draw), so one static buffer is enough.
#define LINE_MAX 480
static uint32_t line[LINE_MAX];

// Framebuffer writes normally target the process that called the display
// syscall; the PSP emulator fallback draws from reVita's own thread instead.
static SceUID targetPid = -1;

void renderer_setTargetProcess(SceUID pid){
	targetPid = pid;
}

static inline void fbCopy(void* dst, const void* src, SceSize len){
	if (targetPid >= 0)
		ksceKernelCopyToUserProc(targetPid, dst, src, len);
	else
		ksceKernelMemcpyKernelToUser(dst, src, len);
}

#define UI_CORNER_RADIUS 9
#define ANIMATION_TIME  120000
static const unsigned char UI_CORNER_OFF[UI_CORNER_RADIUS] = {9, 7, 5, 4, 3, 2, 2, 1, 1};

bool readPixel(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const unsigned char* img){
	uint32_t realW = ((w - 1) / 8 + 1) * 8;
	uint32_t idx = (realW * y + x) / 8;
	uint8_t bitN = 7 - ((realW * y + x) % 8);
	return READ(img[idx], bitN);
}

void drawPixel(int32_t x, int32_t y, uint32_t color){
	if (x >= 0 && x < fbWidth && y >= 0 && y < fbHeight)
		fbCopy((void*)&fb_base[y * fbPitch + x], &color, sizeof(color));
}

void renderer_blankFrame(){
	for (int i = 0; i < LINE_MAX; i++)
		line[i] = BLACK;
	for (uint32_t i = 0; i < fbHeight; i++)
		for (uint32_t x = 0; x < fbWidth; x += LINE_MAX){
			uint32_t n = min(fbWidth - x, (uint32_t)LINE_MAX);
			fbCopy((void*)&fb_base[i * fbPitch + x], line, n * sizeof(uint32_t));
		}
}

void renderer_drawChar(char character, int x, int y){
	unsigned char c = (unsigned char)character;	// every byte value has a glyph
	renderer_drawImage(x, y, FONT_WIDTH, FONT_HEIGHT, &font[c * FONT_GLYPH_BYTES]);
}

void renderer_drawCharIcon(char character, int x, int y){
	unsigned char c = (unsigned char)character;
	if (c >= ICON_ID__NUM)
		return;
	renderer_drawImage(x, y - 1, ICON_W, ICON_H, &ICON[c * ICON_BYTES]);
}

void renderer_drawImage(int32_t x, int32_t y, int32_t w, int32_t h, const unsigned char* img){
	uint32_t idx = 0;
	uint8_t bitN = 0;
	for (int j = 0; j < h; j++){
		for (int i = 0; i < w; i++){
			if (bitN >= 8){
				idx++;			
				bitN = 0;
			}
			if (READ(img[idx], (7 - bitN)))
				drawPixel(x + i, y + j, color);
			bitN++;
		}
		if (bitN != 0){
			idx++;			
			bitN = 0;
		}
	}
}

void renderer_drawString(int x, int y, const char *str){
	size_t len = strlen(str);
	for (size_t i = 0; i < len; i++){
		if (str[i] == ' ') continue;
		if (str[i] == '$'){
			if (i + 1 >= len) break;	// stray '$' at the end of the string
			renderer_drawCharIcon(str[i + 1], x + i * CHA_W, y);
			i++;
		} else {
			renderer_drawChar(str[i], x + i * CHA_W, y);
		}
	}
	if (isStripped)
		renderer_drawRectangle(x, y + CHA_H / 2, len * CHA_W, 2);
}

void renderer_drawStringF(int x, int y, const char *format, ...){
	char str[512] = { 0 };
	va_list va;

	va_start(va, format);
	vsnprintf(str, sizeof(str), format, va);
	va_end(va);

	renderer_drawString(x, y, str);
}

void renderer_drawRectangle(int32_t x, int32_t y, int32_t w, int32_t h) {
	if (w <= 0 || h <= 0)
		return;
	int32_t x0 = max(x, 0), y0 = max(y, 0);
	int32_t x1 = min(x + w, (int32_t)fbWidth), y1 = min(y + h, (int32_t)fbHeight);
	if (x0 >= x1 || y0 >= y1)
		return;
	uint32_t n = min(x1 - x0, LINE_MAX);
	for (uint32_t i = 0; i < n; i++)
		line[i] = color;
	for (int32_t j = y0; j < y1; j++)
		for (int32_t xx = x0; xx < x1; xx += n)
			fbCopy((void*)&fb_base[j * fbPitch + xx], line,
				min(x1 - xx, (int32_t)n) * sizeof(uint32_t));
}

void renderer_drawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
	int32_t xinc, yinc, x, y;
	int32_t dx, dy, e;
	dx = abs(x2 - x1);
	dy = abs(y2 - y1);
	if (x1 < x2)
		xinc = 1;
	else
		xinc = -1;
	if (y1 < y2)
		yinc = 1;
	else
		yinc = -1;
	x = x1;
	y = y1;
	drawPixel(x, y, color);
	if (dx >= dy) {
		e = (2 * dy) - dx;
		while (x != x2) {
			if (e < 0) {
				e += (2 * dy);
			} else {
				e += (2 * (dy - dx));
				y += yinc;
			}
			x += xinc;
			drawPixel(x, y, color);
		}
	} else {
		e = (2 * dx) - dy;
		while (y != y2) {
			if (e < 0) {
				e += (2 * dx);
			} else {
				e += (2 * (dx - dy));
				x += xinc;
			}
			y += yinc;
			drawPixel(x, y, color);
		}
	}
}

void renderer_drawLineThick(int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t thickness){
	renderer_drawLine(x1, y1, x2, y2);
	if (abs(x1 - x2) > abs(y1 - y2)){
		int wy = thickness + floorSqrt(thickness * (((float) abs(y1 - y2)) / abs(x1 - x2)));
		for (int i = 0; i < wy; i++) {
			renderer_drawLine(x1, y1 - i, x2, y2 - i);
			renderer_drawLine(x1, y1 + i, x2, y2 + i);
		}
	} else {
		int wx = thickness + floorSqrt(thickness * (((float) abs(x1 - x2)) / abs(y1 - y2)));
		for (int i = 0; i < wx; i++) {
			renderer_drawLine(x1 - i, y1, x2 - i, y2);
			renderer_drawLine(x1 + i, y1, x2 + i, y2);
		}
	}
}

void renderer_setFB(const SceDisplayFrameBuf *param){
	fbWidth = param->width;
	fbHeight = param->height;
	fb_base = param->base;
	fbPitch = param->pitch;
}

void renderer_writeFromVFB(int64_t tickOpened, bool anim){
	if (fb_base == NULL || fbWidth < uiWidth || fbHeight < uiHeight)
		return;	// framebuffer cannot hold the menu (and unsigned maths below would wrap)
	int64_t tick = ksceKernelGetSystemTimeWide();

	uint32_t ui_x = (fbWidth - uiWidth) / 2;
	uint32_t ui_y = (fbHeight - uiHeight) / 2;

	float multiplyer = 0;
	if (ANIMATION_TIME >= tick - tickOpened && anim)
		multiplyer = ((float)(ANIMATION_TIME - (int)(tick - tickOpened))) / ANIMATION_TIME;

	int32_t ui_yAnimated = ui_y - (uiHeight + ui_y) * multiplyer;
	uint32_t ui_yCalculated = max(ui_yAnimated, 0);
	uint32_t ui_cutout = ui_yAnimated >= 0 ? 0 : -ui_yAnimated;

	rendererv_syncPalette();
	for (int i = 0; i < uiHeight - ui_cutout; i++){
		uint32_t off = 0;
		if (i < UI_CORNER_RADIUS){
			off = UI_CORNER_OFF[i];
		} else if (i > uiHeight - UI_CORNER_RADIUS) {
			off = UI_CORNER_OFF[uiHeight - i];
		}
		rendererv_expandRow(i + ui_cutout, line);
		fbCopy(
			(void*)&fb_base[(ui_yCalculated + i) * fbPitch + ui_x + off],
			&line[off],
			sizeof(uint32_t) * (uiWidth - 2 * off));
	}
}

void renderer_setColor(uint32_t c){
	color = c;
}

void renderer_setStripped(bool flag){
	isStripped = flag;
}

void renderer_init(){
}

void renderer_destroy(){
}
