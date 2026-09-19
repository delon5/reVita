# reVita memory review

> **Status: implemented on this branch.** The sections below are the original review.
> What was actually changed, and the measured result, is summarised first.

## Implemented changes and measured result

Built with VitaSDK 2026.08 (GCC 15.2), `arm-vita-eabi-size` of the kernel module ELF:

| | text | data | bss | image total | runtime memblocks | kernel RAM, menu closed | kernel RAM, menu open |
|---|---|---|---|---|---|---|---|
| Before (`01a872e`, -O3) | 135400 | 12924 | 35128 | 183452 | 94208 always + 524288 while the menu is open | 277660 | 801948 |
| After (this branch, -Os) | 79696 | 12908 | 143248 | 235852 | none (transient only: 12288 or 4096 for an INI load/save, 32768 during a save backup) | 235852 | 235852 |

Steady state drops by 41808 B (15 %); the menu-open peak drops by 566096 B (71 %), and nothing is
allocated when the menu opens, so the "Buy more RAM !" popup no longer exists.

What changed (all in `reVita/src`, mirrored into `reVitaHeadless/src` where the file exists there):

1. **Virtual framebuffer** (`gui/rendererv.[ch]`, `gui/renderer.c`, `gui/gui.[ch]`): static 4-bit palette-indexed
   buffer (480 x 272 / 2 = 65280 B of .bss) instead of a 512 KB memblock per menu open. Every colour drawn into
   it is a `theme[]` entry, so the buffer is lossless; rows are expanded to 32-bit ARGB through a 1920 B line
   buffer inside the existing per-row copy. Drawing primitives clip instead of wrapping; `gui_open()` cannot fail.
2. **Remap input cache** (`remap.[ch]`): static arrays instead of a permanent 94208 B memblock, and only for the
   two hooks that are actually buffered (the Region hooks never used their half): 57344 B of .bss. The history
   depth stays at 64 so that apps asking for up to 64 buffers get exactly what they got before (a 16-deep cache was
   tried and rejected in review for that reason). `remap_touch` clamps before pointer arithmetic and guards an empty
   cache; `remap_ctrl_readBuffer` no longer rejects the oldest cached entry (an off-by-one that left the first
   sample after a reset un-remapped).
3. **Dead 16 KB log buffer** (`log.c`): compiled only with `LOG_DISC`.
4. **Font and icon tables** (`gui/img/*`): defined once in `font-data.c` / `icons-font-data.c` instead of twice
   (they were `static const` in headers included by both renderers): 17920 B of rodata.
5. **`-Os`** instead of `-O3` (`reVita/CMakeLists.txt`, `reVitaHeadless/CMakeLists.txt`): text 118120 -> 79540 B
   on the otherwise identical tree. `--gc-sections` was tried and rejected: it discards the exported syscall
   functions and `module_stop`, and `vita-elf-create` then fails.
6. **Kernel memory-safety fixes** found by the review: bounded INI writer with an overflow flag and a 12288 B
   profile buffer (worst case about 11.4 KB), so a profile can never overflow kernel memory again (the writer
   formats into a scratch buffer first, because libk's PDCLib `vsnprintf` does not bound literal format
   characters); a failed save shows "Profile NOT saved"; `ini_nextEntry` no longer recurses once per section line
   (a file of a few dozen consecutive section lines overflowed the kernel stack); `parseBGR` no longer reads past an
   empty theme value; `fio_writeFile` checks the write result; `sscanf` widths that match the destination buffers;
   `fio_readFile` NUL-terminates; rule index range check; the action-name table completed (it was one entry short and
   `strcmp(NULL, ...)` was reachable from a typo in a profile file); emulated-touch report capacity check; file copy
   cleanup paths fixed and one 32 KB transfer buffer per copied tree instead of 128 KB per file; the swapped
   "savegame backup" / "savegame restore" remap actions; `menu-debug-buttons.c` drawing with the wrong renderer.
7. **Build**: ported to the current vitasdk headers (`ksceKernelCopyToUser`/`CopyFromUser` take pointers);
   `-Wl,--defsym=__sce_headroom=0x1000` so the linker leaves room after the text segment for the SCE module info
   (`reVitaHeadless` did not link without it, `reVita` only linked by size coincidence); verbose logging is an
   opt-in CMake option (`-DREVITA_LOG_DEBUG=ON`) instead of always on for root builds.

Visible side effect to be aware of: popup backgrounds and the "clear screen" blank frame are now drawn with
the intended colour (opaque black or the theme background). The old code filled them with `memset()` of the low
byte of the colour, which produced alpha 0 black for the dark theme; on the SceShell overlay plane that used to
show as transparent.

Not done (deliberately): shrinking the three `Profile` copies or `MenuEntry` (under 4 KB, and the `Profile` layout
is exported to `reVitaMotion`); dirty-frame tracking and row-buffered popup drawing (CPU only, no RAM).

Not verified on hardware: everything above compiles and links, and the logic was reviewed adversarially, but
nothing was run on a Vita. The 4bpp nibble paths (odd x, the rounded corners) and the menu animation are the
first things to check on a device.

---

Scope: `reVita/` (kernel plugin, `reVita.skprx`), `reVitaHeadless/` and `reVitaMotion/`, at commit `01a872e`.
Goal: find where kernel RAM can be saved, and explain and remove the "Buy more RAM !" popup.

Method: every finding below was produced by reading the source, then independently re-verified against the code
by two separate passes (one checking that the claim is literally true of the cited lines and that the arithmetic is
right, one attacking the proposed fix for PS Vita kernel validity and for regressions). Findings that failed either
pass are listed under "Checked and not a problem". No VITASDK toolchain was available, so nothing was compiled or
run on hardware; struct sizes were verified with `clang --target=armv7a-none-eabi` against the vitasdk header
definitions, and the INI sizes with a byte-exact simulation of `generateINIProfile()`.

## Verified numbers used throughout

Struct sizes (ARM EABI, compiler-verified):

| Type | Bytes | Notes |
|---|---|---|
| `SceCtrlData` | 32 | vitasdk `psp2common/ctrl.h`, `VITASDK_BUILD_ASSERT_EQ(0x20)` |
| `SceTouchData` | 144 | 8 x 16-byte `SceTouchReport` + 16 |
| `SceMotionState` | 248 | |
| `RemapRule` | 60 | |
| `ProfileEntry` | 28 | |
| `Profile` | 2744 | x3 globals (`profile`, `profile_global`, `profile_home`) = 8232 B |
| `EmulatedTouchEvent` | 40 | `int64_t tick` forces 8-byte alignment |
| `EmulatedTouch` | 248 | `remap.c` keeps 4 (992 B), `gui.c` keeps 2 more plus 2 `SceTouchData` (784 B) |
| `MenuEntry` | 36 | |
| `Menu` | 64 | |
| `BtnInfo` | 24 | x23 = 552 B |
| `INI_READER` | 248 | one static copy in `ini.c`, one stack copy per parser |

Allocations:

| Allocation | Bytes | Lifetime |
|---|---|---|
| Virtual framebuffer (`rendererv.c:113`) | 480 x 272 x 4 = 522240, page-rounded to 524288 (512 KB) | every menu open, freed at menu close |
| Remap cache (`remap.c:20`, `MEM_SIZE`) | 32x64x5x2 + 144x64x4x2 = 20480 + 73728 = 94208 (already page-aligned, 92 KB) | whole module lifetime |
| INI read/write buffers (`profile.c`, `settings.c`, `theme.c`, `hotkeys.c`) | 4096 each | per load/save |
| Save copy buffer (`fio.c:110`) | 131072 (128 KB) | per file copied |
| Disc log buffer (`log.c:8`) | 16384 of .bss | always, never used |

Generated profile INI size against the 4096-byte buffer it is written into (simulation of `generateINIProfile()`,
43 entries with worst-case values = 979 B header):

| Rules | 1 button -> 1 button rules | Custom touch zone/point rules | All-23-button rules |
|---|---|---|---|
| 5 | 1754 | 2339 | 2969 |
| 10 | 2529 | 3699 | 4959 (overflow) |
| 13 | 2997 | 4518 (overflow) | 6156 |
| 20 | 4089 | 6429 | 8949 |
| 21 | 4244 (overflow) | | |
| 25 | 4869 | 7794 | 10944 |

The GUI allows 24 rules and the INI parser 25, so overflow is reachable from the menu alone.

## Why Buy more RAM appears

The popup has exactly one emitter. `gui_open()` at `reVita/src/gui/gui.c:490-495` runs `if (rendererv_allocVirtualFB() < 0){ LOG("memory allocation for menu failed\n"); gui_popupShowDanger("Error", "Buy more RAM !", TTL_POPUP_LONG); return; }`, and `rendererv_allocVirtualFB()` at `reVita/src/gui/rendererv.c:113` is a single `ksceKernelAllocMemBlock("vfb_base", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, (uiWidth*uiHeight*sizeof(uint32_t) + 0xfff) & ~0xfff, NULL)` with `uiWidth = 480, uiHeight = 272` (`gui.h:7-8`, `gui.c:541`). That is 480 * 272 * 4 = 522240 B, page-rounded to (522240 + 4095) & ~4095 = 524288 B = 128 pages of the kernel RAM partition that every other skprx shares. The request is issued on every `HOTKEY_MENU` press (`main.c:582-585` -> `gui_open()`), mid-game, after ucdc, PSVShell and the running title have already carved the partition up, and the block is freed again in `gui_close()` (`gui.c:505` -> `rendererv.c:123`). Nothing degrades: on a negative return the function shows the popup and returns, so the menu is simply unavailable. For scale, this one transient request is 5.6x larger than everything else reVita keeps resident (the remap cache, see below), and it is the only allocation in the plugin big enough to fail on a partition that still serves the routine 4096 B INI buffers.

## Recommended fix for the popup

Two of three judges picked the palette-indexed 4bpp buffer; the third picked allocate-once with an 8bpp graft. All three agree on the merged shape: an indexed buffer, allocated once (never at menu open), with design 3's error-path hardening and design 4's clipping, and without the band loop or CDRAM-first ladder. The buffer is lossless because every colour that reaches `vfb_base` is a `theme[]` entry: `rendererv.c:60` stores `color` (set only via `rendererv_setColor`, `rendererv.c:104-106`), `rendererv.c:101` stores the `clr` argument of `rendererv_drawRectangle`, and a grep of `reVita/src/gui` finds no call site of either whose argument is not `theme[...]` (the internal call at `rendererv.c:82` forwards `color`). `theme[]` has `THEME_COLOR__NUM = 14` entries (`theme.h:9-26`). The only literal-colour writer, `clear()` at `rendererv.c:32-39`, has no caller. The only reader is `renderer.c:205-208`, which already copies one row at a time.

### Implementation plan

1. `rendererv.h:9` `extern uint32_t* vfb_base;` becomes `extern uint8_t* vfb_base;`. Add `#define VFB_BPP 4`, `#define VFB_STRIDE(w) ((w) * VFB_BPP / 8)` (240 B per row at 4bpp). Change `rendererv_drawImage`/`rendererv_drawRectangle` parameters (`rendererv.h:14,17`) from `uint32_t` to `int32_t` so negative coordinates can be clipped instead of wrapping. Add `void rendererv_expandRow(uint32_t y, uint32_t *dst); void rendererv_syncPalette(void);`.

2. `rendererv.c`: replace the memblock with static storage, delete `clear()` (`:32-39`) and `vreadPixel()` (`:25-30`), and route every pixel store through one clipped helper.

```c
/* rendererv.c: replaces lines 16-20, 25-39, 112-124 */
#include "../fio/theme.h"                 /* theme[], THEME_COLOR__NUM == 14 */
#define VFB_IDX_CUSTOM 14
#define VFB_IDX_BLACK  15

static uint8_t  vfb_storage[272 * VFB_STRIDE(480)];   /* 272 * 240 = 65280 B .bss */
uint8_t*        vfb_base = vfb_storage;
uint32_t        uiWidth, uiHeight;
static uint8_t  colorIdx = VFB_IDX_BLACK;             /* replaces `static uint32_t color;` */
static uint32_t customColor = 0xFF000000;
static uint32_t pal[16];
static bool     isStripped;

static uint8_t colorToIdx(uint32_t c){
    for (int i = 0; i < THEME_COLOR__NUM; i++) if (theme[i] == c) return (uint8_t)i;
    if (c == 0xFF000000) return VFB_IDX_BLACK;
    if (customColor != c) LOG("rendererv: non-theme colour %08X\n", c);
    customColor = c;
    return VFB_IDX_CUSTOM;
}
void rendererv_syncPalette(void){
    for (int i = 0; i < THEME_COLOR__NUM; i++) pal[i] = theme[i];
    pal[VFB_IDX_CUSTOM] = customColor;
    pal[VFB_IDX_BLACK]  = 0xFF000000;
}
/* even x = low nibble, odd x = high nibble; unsigned compare rejects negatives (gui.c:162) */
static inline void vput(int32_t x, int32_t y, uint8_t idx){
    if ((uint32_t)x >= uiWidth || (uint32_t)y >= uiHeight) return;
    uint8_t *b = &vfb_base[(uint32_t)y * VFB_STRIDE(uiWidth) + ((uint32_t)x >> 1)];
    *b = (x & 1) ? (uint8_t)((*b & 0x0F) | (idx << 4)) : (uint8_t)((*b & 0xF0) | idx);
}
static inline void fillRowIdx(uint32_t y, uint32_t x0, uint32_t x1, uint8_t idx){ /* [x0,x1) clipped */
    uint8_t *row = &vfb_base[y * VFB_STRIDE(uiWidth)];
    if (x0 & 1){ row[x0 >> 1] = (row[x0 >> 1] & 0x0F) | (idx << 4); x0++; }
    if (x1 & 1){ x1--; row[x1 >> 1] = (row[x1 >> 1] & 0xF0) | idx; }
    if (x1 > x0) memset(&row[x0 >> 1], idx | (idx << 4), (x1 - x0) >> 1);
}
void rendererv_expandRow(uint32_t y, uint32_t *dst){
    const uint8_t *src = &vfb_base[y * VFB_STRIDE(uiWidth)];
    for (uint32_t k = 0; k < uiWidth / 2; k++){ uint8_t b = src[k]; dst[2*k] = pal[b & 0x0F]; dst[2*k+1] = pal[b >> 4]; }
}
void rendererv_drawImage(int32_t x, int32_t y, int32_t w, int32_t h, const unsigned char* img){
    uint32_t idx = 0; uint8_t bitN = 0;                       /* same bit walk as rendererv.c:50-68 */
    for (int j = 0; j < h; j++){
        for (int i = 0; i < w; i++){
            if (bitN >= 8){ idx++; bitN = 0; }
            if (READ(img[idx], (7 - bitN))) vput(x + i, y + j, colorIdx);   /* was vfb_base[...] = color */
            bitN++;
        }
        if (bitN != 0){ idx++; bitN = 0; }
    }
}
static void drawRectIdx(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t idx){
    int32_t x0 = max(x, 0), y0 = max(y, 0);
    int32_t x1 = min(x + w, (int32_t)uiWidth), y1 = min(y + h, (int32_t)uiHeight);
    if (x0 >= x1 || y0 >= y1) return;                           /* replaces whole-rect reject at :97 */
    for (int32_t j = y0; j < y1; j++) fillRowIdx(j, x0, x1, idx);
}
void rendererv_drawRectangle(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t clr){ drawRectIdx(x, y, w, h, colorToIdx(clr)); }
void rendererv_setColor(uint32_t c){ colorIdx = colorToIdx(c); }
/* rendererv_drawString line 82: if (isStripped) drawRectIdx(x, y + CHA_H/2, strlen(str)*CHA_W, 2, colorIdx); */
int  rendererv_allocVirtualFB(void){ return 0; }   /* keep the symbols so gui.c compiles; no allocation */
int  rendererv_freeVirtualFB(void){ return 0; }
```

3. `renderer.c:184-210` keeps its animation maths (`:185-196`) and corner offsets (`:200-204`) verbatim; only the source operand of the per-row copy changes.

```c
static uint32_t line[480];                       /* 1920 B .bss; serialised by mutex_gui_uid (gui.c:332) */
void renderer_writeFromVFB(int64_t tickOpened, bool anim){
    /* lines 185-196 unchanged: ui_x, ui_y, multiplyer, ui_yAnimated, ui_yCalculated, ui_cutout */
    rendererv_syncPalette();                      /* once per frame */
    for (int i = 0; i < uiHeight - ui_cutout; i++){
        uint32_t off = 0;                         /* lines 200-204 unchanged */
        if (i < UI_CORNER_RADIUS) off = UI_CORNER_OFF[i];
        else if (i > uiHeight - UI_CORNER_RADIUS) off = UI_CORNER_OFF[uiHeight - i];
        rendererv_expandRow(i + ui_cutout, line);
        ksceKernelMemcpyKernelToUser((uintptr_t)&fb_base[(ui_yCalculated + i) * fbPitch + ui_x + off],
                                     &line[off],  /* was &vfb_base[(i + ui_cutout) * uiWidth + off] */
                                     sizeof(uint32_t) * (uiWidth - 2 * off));
    }
}
```

4. `gui.c:490-502`: delete lines 491-495 (the alloc test, LOG, popup, return). `gui.c:505`: delete `rendererv_freeVirtualFB();`. `gui.c:77`: delete the dead `SceUID mem_uid;`. `gui.c:568-572`: add `rendererv_destroy();` next to `renderer_destroy();` (it is never called today, `rendererv.c:131-132`).

5. `menu-debug-buttons.c:32,35,47` call `renderer_setColor` from an `onDraw`; change to `rendererv_setColor` (pre-existing bug, colours never reach the menu buffer).

6. `main.c`: no change. `ksceDisplaySetFrameBufInternal_patched` (`main.c:401-436`) still calls `gui_draw(pParam)` at `:422`; the no-flicker double submit (`:424-431`) is untouched. `README.md:123`: remove the popup entry.

7. Alternative if a 65 KB static array is unwanted: allocate once. In `rendererv_init` (`rendererv.c:126-129`, called from `gui_init` at `main.c:690`, before hooks at `:735` and the thread at `:741`) call an idempotent `rendererv_ensureVirtualFB()` requesting `(272 * 240 + 0xfff) & ~0xfff = 65536` B = 16 pages of `SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW`; retry from `gui_open` and keep the popup as the last-resort path; free only in `rendererv_destroy`. Initialise `SceUID vfb_uid = -1; uint8_t* vfb_base = NULL;`, test `uid < 0` (not `ret > 0` as at `rendererv.c:114`, which disagrees with `gui.c:491`), check `ksceKernelGetMemBlockBase` and free on failure, guard the free with `if (vfb_uid < 0) return 0;`. Do not add CDRAM rungs in front of this: CDRAM blocks are 256 KiB-granular, so a 65536 B request would cost 262144 B of VRAM; if a second rung is wanted at all, use `SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_CDRAM_L1WBWA_RW` (0x40404006, L1-cached) last, never the uncached 0x40408006, and never `KERNEL_ROOT_PHYCONT_RW`.

8. Optional follow-up: convert the 34 `rendererv_setColor(theme[X])` call sites to `rendererv_setColorId(X)` to remove the reverse lookup and the slot-14 merge; switch `VFB_BPP` to 8 (130560 B, byte stores, no nibble code) if the nibble path cannot be tested on hardware before release.

### Memory before and after

Note on the remap block: `MEM_SIZE` at `remap.c:20` is `(0xfff + CACHE_CTRL_SIZE + CACHE_TOUCH_SIZE) & ~0xfff` = (4095 + 20480 + 73728) & ~4095 = 98303 & ~4095 = 94208 B. The 98304 figure carried in earlier notes is a rounding slip: 94208 = 0x17000 is already page-aligned, so the mask adds nothing.

| State | Today | After (static 4bpp) |
|---|---|---|
| Steady state, menu closed, kernel memblocks | 94208 B (remap cache, `remap.c:1308-1309`) | 94208 B |
| Steady state, extra module .bss for the VFB path | 0 B | 65280 + 1920 + 64 = 67264 B |
| Peak, menu open | 94208 + 524288 = 618496 B (plus 4096 B transient at autosave, `profile.c:538`) | 94208 + 67264 = 161472 B (same 4096 B transient) |
| Requested at each menu open | 524288 B (128 pages) | 0 B |
| Peak reduction | | 618496 - 161472 = 457024 B |

If the dead `log_buf` and the font/icon duplicates from section 3 land in the same patch, the module image shrinks by 16384 + 10240 + 7680 = 34304 B, so the net static growth is 67264 - 34304 = 32960 B. With the remap cache shrink as well, reVita's resident total for these items drops to 14336 + 67264 = 81600 B against today's 94208 B closed / 618496 B open.

### Residual failure modes

- The reservation moves to module load: 67264 B more .bss must be mapped by the kernel loader at boot. If that fails the whole plugin fails to load silently instead of only the menu. 67 KB is the size of a single INI-buffer page times 16, so this is far less likely than today's 128-page request, but it is not zero. The allocate-once memblock alternative (step 7) degrades only the menu.
- CPU: roughly 130560 palette lookups plus nibble read-modify-writes per frame while the menu is open, estimated at about 1 ms per `SetFrameBuf` on Cortex-A9 (estimate; nothing can be compiled or timed in this container). Under `PR_MO_NO_FLICKER` (`main.c:424-431`) this could occasionally cost a vblank; 8bpp halves the nibble work.
- Nibble edge cases in `fillRowIdx`/`vput` at odd `x` (e.g. `drawIndent` at x = 13, w = 448, `gui.c:210`) and odd corner offsets 9/7/5/3/1 are the main correctness risk; test them on hardware.
- A future caller passing a colour outside `theme[]` is merged into slot 14; two distinct such colours in one frame would render wrong. There are zero such callers today (grep), and `colorToIdx` logs any.
- `gui_close` autosave (`gui.c:517-525`) still allocates its 4096 B INI block and ignores `profile_save`'s return; that is a separate finding.

## Memory savings, ranked

| # | Item | Where (file:line) | Bytes now | Bytes after | Effort | Risk |
|---|---|---|---|---|---|---|
| 1 | Virtual framebuffer to static 4bpp | `rendererv.c:113`, `gui.c:491/505` | 524288 B per menu open (KD memblock) | 65280 + 1984 = 67264 B .bss, 0 B per open | Medium | Medium |
| 2 | Remap history cache: BUFFERS_NUM 64 -> 16, touch cache 4 -> 2 hooks, .bss instead of memblock | `remap.h:7`, `remap.c:18-20`, `remap.c:1308-1310` | 94208 B permanent memblock | 32*16*5*2 + 144*16*2*2 = 5120 + 9216 = 14336 B .bss; saves 79872 B | Medium | Medium |
| 3 | Dead touch cache half only (subset of 2, zero semantic change) | `remap.c:19`, `remap.c:1319-1322` | 73728 B touch cache | 36864 B; `MEM_SIZE` = (0xfff + 20480 + 36864) & ~0xfff = 57344 B; saves 36864 B | Low | Low |
| 4 | Save backup/restore transfer buffer: one allocation per tree, 32 KB | `fio.c:8`, `fio.c:110-116,150` | 131072 B transient, N times per backup | 32768 B transient, once; peak saving 98304 B | Low | Low |
| 5 | Dead 16 KB disc-log buffer | `log.c:8` | 16384 B .bss | 0 B | Trivial | None |
| 6 | `font[]` defined twice (static const in a header) | `font.h:4`, `renderer.c:9/56`, `rendererv.c:9/43` | 2 * 10240 = 20480 B rodata | 10240 B; saves 10240 B | Low | None |
| 7 | `ICON[]` defined twice | `icons-font.h:138`, `renderer.c:11/60`, `rendererv.c:11/47` | 2 * 7680 = 15360 B rodata | 7680 B; saves 7680 B | Low | None |
| 8 | GUI copy of emulated-touch state passed by value | `gui.c:34-35`, `gui.c:355-358`, `remap.c:1119` | 784 B .bss + 392 B stack copy per touch sample | ~64-116 B per panel, pointer args | Low | Low |
| 9 | `EmulatedTouchEvent` repack | `remap.h:132-141` | 40 B * 6 * 6 instances = 1440 B | 24 B each; saves 96 * 6 = 576 B | Low | Low |

### 1. Virtual framebuffer

Covered in section 2. Arithmetic: 480 * 272 = 130560 px; at 4 bits per pixel 130560 / 2 = 65280 B; today 130560 * 4 = 522240 -> 524288 B. Saving per open: 524288 - 65280 = 459008 B (87.5%). The 8bpp fallback is 130560 -> (as memblock) 131072 B, saving 393216 B.

### 2. Remap history cache

`remap.h:7 #define BUFFERS_NUM 64`; `remap.c:18-19` `CACHE_CTRL_SIZE = 32*64*5*2 = 20480`, `CACHE_TOUCH_SIZE = 144*64*4*2 = 73728`; allocated once at `remap.c:1308-1309` and held until `remap_destroy` (`remap.c:1341-1344`). Callers are already clamped to what is cached (`main.c:252 ret = min(ret, remap_ctrl_getBufferNum(port))`, `remap.c:1156 nBufs = min(nBufs, cacheTouch[hookId][port].num)`), so a shallower cache is semantically supported. Concrete change: split the constant. Keep a `MAX_HOOK_BUFS 64` for the bypass guards at `main.c:211 if (ret < 1 || ret > BUFFERS_NUM) return ret;` and `main.c:318` (the SceCtrl API maximum is 64; shrinking that guard would silently un-remap any game that requests more than the new depth), and introduce `CACHE_BUFS 16` for the array dimensions and the shift loops at `remap.c:962-966` and `remap.c:1141-1145`. Clamp the returned count only after the newest sample is taken from `&ctrl[ret-1]` (`main.c:249/260`) and `&pData[nBufs-1]` (`main.c:335/344`). Replace the memblock with `static SceCtrlData ctrlBuf[PORTS_NUM][PROC_NUM][CACHE_BUFS]; static SceTouchData touchBuf[2][SCE_TOUCH_PORT_MAX_NUM][CACHE_BUFS];`, point `cacheCtrl/cacheTouch .buffers` at them in `remap_init`, delete the alloc at `remap.c:1308-1310` and the free at `remap.c:1343` (this also removes the unchecked-allocation crash in section 4). Replace the element-by-element shift with one `memmove`; do not use a ring index, because `remap_touch` returns a pointer to a contiguous run (`remap.c:1158`) that `main.c:336/345` copies in one shot. Guard `remap_touch`'s fast path (`remap.c:1136` reads `buffers[num-1]` when `num == 0`). Mirror in `reVitaHeadless/src/remap.c:17-19, 1282-1317` and `reVitaHeadless/src/main.c:204, 291`. Depth 8 would save 6 KB more (7168 B total) but is aggressive; 16 is the recommended default.

### 3. Dead touch cache half

`cacheTouch` is `[TOUCH_HOOKS_NUM=4][2]` (`remap.c:81`, `main.h:47`) but `remap_touch`, its only user (`remap.c:1135-1158`), is called only for `hookIdx < 2` (`main.c:334-335`, `:343-344`); region hooks go to `remap_touchRegion` (`remap.c:1161-1165`, "Do not use buffering for regions"). Add `#define TOUCH_CACHE_HOOKS_NUM 2` and use it for the array dimension, `CACHE_TOUCH_SIZE` (`remap.c:19`), the init loop (`remap.c:1319`) and, critically, the reset loop at `remap.c:1178-1181` (otherwise it writes `cacheTouch[2..3][i].num` past the shrunk array). `newEmulatedTouchBuffer[TOUCH_HOOKS_NUM]` (`remap.c:83`) must stay at 4. Subsumed by item 2 if that lands.

### 4. Save backup/restore buffer

`fio.c:8 #define TRANSFER_SIZE (128 * 1024)`; `fio.c:110-116` allocates `(131072 + 0xfff) & ~0xfff = 131072` B per `fio_copyFile`, freed at `fio.c:150`; `fio_copyDir` (`fio.c:189-193`) calls it once per file recursively from `sysactions.c:91/112`. Allocations are sequential so peak is 131072 B, but each file is a fresh 32-page request and any single failure aborts the whole copy ("Failed !", `sysactions.c:97/118`). Allocate once in `sysactions_saveBackup/saveRestore`, thread `char *buf, int bufSize` through `fio_copyDir -> fio_copyFile`, and use 32768 B (page-exact). Note `sysactions_saveBackup/saveRestore` are also called from the ctrl hook via `remap.c:387-388` (which, incidentally, has BACKUP and RESTORE swapped: `REMAP_SYS_SAVE_BACKUP -> sysactions_saveRestore`), so the copy runs in syscall context either way; keep the code free of anything that sleeps.

### 5. Dead log buffer

`log.c:7-8 static uint log_buf_ptr = 0; static char log_buf[16 * 1024];` The only users `log_reset/log_write/log_flush` (`log.c:10,21,30`) are called solely from the `#elif LOG_DISC` branch of the macro (`log.h:40-53`), and `LOG_DISC` is never defined (`CMakeLists.txt:11` is `# add_definitions(-DLOG_DISC)`, commented). Wrap the body of `log.c` in `#ifdef LOG_DISC ... #endif` or drop `src/log.c` from `reVita/CMakeLists.txt:59`. Same in `reVitaHeadless/src/log.c` (kernel) and `reVitaMotion/src/log.c` (user mode, no kernel RAM effect).

### 6 and 7. Duplicated glyph tables

`font.h:4 static const unsigned char font[] = {` (no include guard, 10240 hex bytes = 256 glyphs * 40 B) and `icons-font.h:138 static const unsigned char ICON[] = {` (7680 hex bytes = 128 icons * 60 B) are internal-linkage objects, so each of the two TUs that index them (`renderer.c:56/60`, `rendererv.c:43/47`) emits its own copy; `reVita/CMakeLists.txt:14` has no `-fmerge-all-constants`, and GCC will not merge distinct static arrays without it. Keep `FONT_WIDTH/FONT_HEIGHT`, `ICON_W/ICON_H` and the `ICON_ID` enum in the headers, declare `extern const unsigned char font[256*40]; extern const unsigned char ICON[128*60];`, and move both initialisers into one new `gui/img/img.c` added to `add_executable` at `reVita/CMakeLists.txt:24-60`. `icons.h` (300 B) is referenced only from `gui.c` so it costs one copy today; converting it is hygiene only. Realised saving after 4 KiB segment rounding is 16-20 KB of kernel RAM for the pair.

### 8 and 9. Emulated-touch duplicate and repack

`gui.c:34-35 static EmulatedTouch et[2]; static SceTouchData td[2];` = 2 * 248 + 2 * 144 = 784 B duplicating `remap.c:85-87`, refreshed by `gui_updateEmulatedTouch(SceTouchPortType, EmulatedTouch, SceTouchData)` by value (`gui.h:136`, `gui.c:355-358`) from `remap.c:1119` on every touch buffer, i.e. 248 + 144 = 392 B pushed on the game thread's syscall stack (the caller is `onTouch`, `main.c:315-353`, inside the `ksceTouchPeek/Read` hooks) and 392 B assigned again. `drawEmulatedPointersForPanel` (`gui.c:235-251`) reads only `num`, the two swipe flags, three `TouchPoint`s per report and `report[i].x/y`. Change the signature to `const EmulatedTouch*, const SceTouchData*` and copy a compact struct (~64-116 B per panel). Do not keep raw pointers: the user-space path at `main.c:347` passes a user pointer and `remap.c:1141-1145` shifts the ring so a cache pointer goes stale. Repacking `EmulatedTouchEvent` (`remap.h:132-141`: `int64_t tick` first, `uint8_t` id/port/ruleIdx, packed flags) takes it from 40 B to 24 B, so `EmulatedTouch` drops from 6*40+1 -> 248 B to 6*24+1 -> 152 B, saving 96 * 6 = 576 B; `id` fits `uint8_t` because `etIdCounter` is uint8 mod 127 (`remap.c:88, 1033-1034`).

### Not RAM, but worth knowing (CPU inside the game's SetFrameBuf syscall)

- `drawPixel` (`renderer.c:36-39`) issues one 4-byte `ksceKernelMemcpyKernelToUser` per set pixel; a full popup is at most 78 glyphs * 240 px = 18720 calls per frame (`gui.c:87-88` buffers hold 39 chars each), a touch pointer 2 * 32*32 = 2048 calls (`gui.c:223,225`). Row-buffering `renderer_drawImage` (one copy per glyph row, read the FB row back with `ksceKernelMemcpyUserToKernel` for transparent pixels, clip to `[0,fbWidth)` and respect `fbPitch`) cuts it 12x. Popups draw on an opaque box (`gui.c:282-285`) so their glyph rows need no read-back.
- `gui_draw` (`gui.c:331-339`) re-rasterises all 140416 px every frame and then does 272 row copies of 521768 B total (`renderer.c:198-209`, 522240 minus 2*4*(34+25) = 472 B of corner trim). A dirty flag can skip the raster on idle frames but must be forced on for the blink (`gui.c:179`), the footer scroller (`gui.c:196-203`) and any menu with `onDraw != NULL` (`menu-debug-buttons.c:28`, `menu-controller.c:30`, `menu-debug-hooks.c:14`); the row copies cannot be coalesced because `fbPitch` (960 or 512) never equals `uiWidth` (480). With the 4bpp buffer the raster is already 8x cheaper in bytes, so this is optional.

## Kernel memory-safety issues found on the way

| # | Issue | Where (file:line) | Trigger | Severity |
|---|---|---|---|---|
| A | Emulated-touch report array overflow overwrites `num` then up to 10240 B of .bss | `remap.c:138-148, 152-169, 172-190`; `remap.h:143-146` | 7 rules on one trigger emitting distinct touch points | Critical |
| B | INI writer is unbounded `vsprintf` into a 4096 B block | `ini.c:10-16`, `ini.c:113-119`, `profile.c:18, 538-539` | 21+ ordinary rules (or 8+ all-button rules), every autosave | Critical |
| C | INI reader never NUL-terminates and ignores the read count | `fio.c:42-53` | any INI file >= 4096 B or a short read | High |
| D | `sscanf` widths exceed destination buffers; `[RULE:10]`..`[RULE:24]` overflow deterministically | `ini.c:67-80, 107-109`; `ini.h:5-9` | any profile with more than 10 rules; hand-edited long tokens | High |
| E | Negative or uninitialised rule index writes a 60 B `RemapRule` outside `profile.remaps[]` | `profile.c:428-433`; `ini.c:127-133` | `[RULE:-1]`, `[RULE:X]` in a hand-edited/corrupted INI | High |
| F | Action name table one entry short: `strcmp(NULL, n)` on any unknown name | `profile.c:65-114`; `remap.h:71-74` | typo in `TRIGGER_ACTION`/`EMU_ACTION` | High |
| G | `remap_init` never checks the 94208 B allocation; hooks write through NULL-based pointers | `remap.c:1308-1310, 1315-1322, 971, 1150, 1343`; `reVitaHeadless/src/remap.c:1282-1284` | kernel partition already exhausted at boot | High |
| H | `fio_copyFile` cleanup labels wired backwards | `fio.c:112-115, 121-132, 141-150` | any I/O error during save backup/restore | High |
| I | `rendererv_drawImage/drawRectangle` have no clipping and take `uint32_t` | `rendererv.c:50-68, 96-102`; `gui.c:162` | right-aligned string >= 39 chars (latent, in-block today) | Low |
| J | Stale `vfb_uid/vfb_base` after a failed alloc; `ret > 0` vs `< 0` mismatch; unchecked `GetMemBlockBase` | `rendererv.c:19-20, 114-124`; `gui.c:491, 505` | future call path bypassing the `gui_isOpen` gate | Low |

**A.** `storeTouchPoint` (`remap.c:138-148`), `storeTouchSwipe` (`:152-169`) and `storeTouchSmartSwipe` (`:172-190`) do `et->reports[et->num]... ; et->num++` with no check against `MULTITOUCH_FRONT_NUM` (6, `remap.h:10`). `EmulatedTouchEvent` is 40 B (3 `TouchPoint` = 12, `int id` at 12, `int64_t tick` at 16, 3 bools, `int port` at 28, `int ruleIdx` at 32, padded to 40), so `reports[6]` = 240 B and `uint8_t num` sits at offset 240: the 7th store writes `point.x` over `num`, `num` becomes `(x & 0xFF) + 1`, and later stores index up to `reports[255]`, i.e. 256 * 40 = 10240 B past `et[port]` into `etPrev`, `etIdCounter` (`remap.c:85-88`) and whatever follows. `applyRemap` stores one point per active rule per ctrl call (`remap.c:293-300`), profiles allow 25 rules, and the only cap is on the emit side (`remap.c:1040`). Fix: after the duplicate scan add `if (et->num >= sizeof(et->reports)/sizeof(et->reports[0])) return;` in all three (do not use `MULTITOUCH_BACK_NUM` = 4 for the back panel; both panels use the 6-entry array and `addVirtualTouches` is called with `MULTITOUCH_FRONT_NUM` for both, `remap.c:1125,1129`); make the smart-swipe callers at `remap.c:308, 331, 348` handle a NULL return or return a static scratch event. Mirror in `reVitaHeadless/src/remap.c`.

**B.** `ini_append` (`ini.c:10-16`) is `vsprintf(ini->idx, fmt, va); ini->idx = &ini->idx[strlen(ini->idx)];` and `ini_create(buff, max)` (`ini.c:113-119`) discards `max`. `writeProfile` allocates `BUFFER_SIZE = (1000 + 0xfff) & ~0xfff = 4096` B (`profile.c:18, 538-539`). The PROFILE section is 47 + 914 = 961 B worst case; a 1-button -> 1-button rule is 156 B; an all-23-button rule is 399 B (list = 105 name chars + 23 commas = 128 B per side). Fit: (4096 - 1 - 961) / 156 = 20 ordinary rules; the GUI allows 24 (`profile.c:242`) and the INI parser 25 (`profile.c:423`), so 24 rules = 961 + 24*156 + 1 = 4706 B (610 B past the page) and 25 all-button rules = 961 + 25*399 + 1 = 10937 B (6841 B past). This runs on every `gui_close` autosave (`gui.c:525`) and every manual save (`profile.c:593/612/622/626`). Fix: store the capacity in `struct INI`, make `ini_append` use `vsnprintf(ini->idx, ini->buff + ini->max - ini->idx, ...)` and return false on truncation so `writeProfile` fails instead of writing a clipped file; raise the profile buffer to 12288 B (3 pages, covers 10937) for both `readProfile` (`profile.c:511`) and `writeProfile`. Because `max` becomes live, every `ini_create(buff, 99)` caller (`settings.c:42`, `theme.c:53`, `hotkeys.c:44`, `profile.c:316-317`, and the headless copies) must pass its real 4096/12288. `gui_close` ignores `profile_save`'s return today; add a `gui_popupShowDanger` on failure (the headless build has no popup and should LOG/return false).

**C.** `fio_readFile` (`fio.c:42-53`) does `ksceIoRead(fd, buff, size);` with `size` equal to the whole 4096 B block, never stores a terminator and discards the byte count; `ini_read` then does `strlen(buff)` (`ini.c:123`) and `ini_nextLine` writes `'\0'` at each `'\n'` it finds (`ini.c:55-58`), so a file of >= 4096 B (which B already produces) reads and writes past the page. All four readers go through it (`profile.c:519`, `hotkeys.c:89`, `settings.c:120`, `theme.c:85`). Fix: `int r = ksceIoRead(fd, buff, size - 1); ksceIoClose(fd); if (r < 0) return false; buff[r] = '\0'; return true;` and `memset(buff, 0, size)` after `ksceKernelGetMemBlockBase` in each reader (block contents are not guaranteed zero). Must land together with B's buffer enlargement or large profiles are silently truncated on load instead of crashing.

**D.** `ini.h:5-9` defines `SECTION_SIZE 30`, `SECTION_ATTR_SIZE 2`, `ENTRY_NAME_SIZE 30`, `ENTRY_VALUE_SIZE 150`, `ENTRY_LIST_VALUE_SIZE 10`, but `ini.c:69` scans `"[%127[^:]:%127[^]]"` into `section[30]`/`sectionAttr[2]`, `ini.c:73` `"[%[^]]"` unbounded, `ini.c:80` `"%[^=]=%s"` unbounded into `key[30]`/`value[150]`, and `ini.c:107-109` `strncpy(ini->listVal, ini->_.listEntry, len)` with unbounded `len` into `listVal[10]`. The writer emits `[RULE:%i]` with i up to 24 (`profile.c:342`), so `"10\0"` (3 B) into the 2-byte `sectionAttr` fires for every profile with more than 10 rules and the `strcpy` at `ini.c:71` then spills into `INI_READER.name[0]` (offset 56, struct verified at 248 B). `INI_READER` is a by-value local in every parser (`profile.c:410`, `hotkeys.c:64`, `settings.c:77`, `theme.c:62`), so long hand-edited tokens smash the kernel stack (`0x3000` thread). Fix: `SECTION_ATTR_SIZE 10`, scan with `"[%29[^:]:%9[^]]"`, `"[%29[^]]"`, `"%29[^=]=%149s"`, clamp `len` to `ENTRY_LIST_VALUE_SIZE - 1`. Longest key the plugin writes is 21 chars, longest value 128, longest list item 9 (`HEADPHONE`), so no existing file changes behaviour.

**E.** `profile.c:428-433`: `ruleId = parseInt(ini->sectionAttr); if (ruleId >= REMAP_NUM) continue; ... struct RemapRule* rr = &p->remaps[ruleId];` with no `< 0` check; `parseInt` (`ini.c:127-133`) is `int result; sscanf(c, "%d", &result); return result;` with the return unchecked, so a non-numeric attr yields an uninitialised index. `sizeof(RemapRule) = 60` and `remaps[]` starts at `Profile` offset 40, so `[RULE:-1]` writes fields at `profile - 20 .. profile + 40` (20 B of the preceding global, `titleid[0..28]`, `version`, `remapsNum`). Because of D the reachable negatives are -1..-9 today; after D is fixed larger values become reachable, so fix both. Fix: `int ruleId = 0; if (sscanf(ini->sectionAttr, "%d", &ruleId) != 1 || ruleId < 0 || ruleId >= REMAP_NUM) continue;` and initialise `result = 0` in `parseInt`.

**F.** `REMAP_ACTION_STR[REMAP_ACTION_NUM]` (`profile.c:65-108`) has 42 string initialisers for 43 enum values (`remap.h:71-74`: `REMAP_SYS_TOGGLE_SECONDARY = 41, REMAP_REM_SWAP_TOUCHPADS = 42, REMAP_ACTION_NUM = 43`), so entry [41] is mislabelled `"REM_SWAP_TOUCHPADS"` and entry [42] is NULL. `getActionId` (`profile.c:109-114`) loops to `REMAP_ACTION_NUM` and reaches `strcmp(NULL, n)` for any name that matches none of the first 42, a NULL dereference in kernel from a typo in a user-editable INI (also reachable via `SHARED.INI`, `profile.c:625`). The `%s` of NULL at `profile.c:356/379` for action 42 is latent because nothing in the tree ever produces action 42 (`menu-remap.c:485` exposes only `REMAP_SYS_TOGGLE_SECONDARY`). Fix with compatibility: append a distinct 43rd string (or alias `"REM_SWAP_TOUCHPADS"` to 41 in `getActionId`) rather than inserting before it, since inserting remaps every existing toggle-secondary rule to the no-op action 42. Add NULL guards in `getActionId/getActionTypeId/getRemapKeyId/getButtonId`. A `_Static_assert` on `sizeof` is a tautology while the array is declared with an explicit bound; declare it `[]` if you want the assert to mean something.

**G.** `remap_init` (`remap.c:1306-1310`): `remap_memId = ksceKernelAllocMemBlock(...MEM_SIZE...); ksceKernelGetMemBlockBase(remap_memId, (void**)&remap_memBase);` with neither return checked. `remap_memBase` is a zero-initialised global (`remap.c:77`), so on failure `cacheCtrl[i][j].buffers = 0 + 2048*(i*2+j)` (offsets 0..18432) and `cacheTouch = 0 + 20480 + 9216*k` (20480..84992), and the first hooked controller poll stores 32 B through them at `remap.c:971` (`cacheCtrl[port][isShell].buffers[idx] = ctrl[0];`); `remap_destroy` (`remap.c:1343`) then frees the negative uid. This draws from the same partition as the VFB, so it is reachable on the affected devices, and the outcome is a kernel data abort at boot. Fix: check both returns, expose `remap_ready` (`remap_memBase != NULL`) and gate the hooks by adding `|| !remap_ready` to the existing `if (!settings[SETT_REMAP_ENABLED].v.b) return ret;` tests in `main.c` before the mutex and before `min(ret, remap_ctrl_getBufferNum(port))`; initialise `remap_memId = -1` and guard the free. Best: adopt savings item 2 (static arrays), which removes the failure path entirely.

**H.** `fio.c:112-115` `if (buff_uid < 0){ ret = buff_uid; goto ERROR_IO; }` falls through `ERROR_IO` (`:141-146`, close both fds, remove dest) into `ERROR` (`:148-150`, `ksceKernelFreeMemBlock(buff_uid)` with a negative uid); the read/write failures at `:121-124` and `:129-132` `goto ERROR` directly, skipping the close/remove, so every I/O error leaks `fdsrc` and `fddst` and leaves a truncated destination, which on the restore path (`sysactions.c:100-117`) is the game's live save. Fix: on alloc failure close both fds and return directly; on read/write error `goto ERROR_IO`; free the memblock once before the closes guarded by `if (buff_uid >= 0)`; treat `written < read` as an error. Callers already treat negative returns as failure (`fio.c:195`, `sysactions.c:91/112`).

**I.** `gui_drawStringFRight` computes `UI_WIDTH - (strlen(str) + 2) * CHA_W - x` (`gui.c:162`), negative for 39+ chars (480 - 41*12 = -12); it flows through `int` parameters into `rendererv_drawImage(uint32_t x, ...)` whose store `vfb_base[(y + j) * uiWidth + x + i]` (`rendererv.c:60`) has no check, so the wrapped index lands 48 B before the row start. With current callers (`y >= 3`, strings under 39 chars) this stays inside the block as visual corruption; it leaves the block only for `y + j == 0`. The 4bpp `vput` helper in section 2 closes it with an unsigned compare; `renderer_drawRectangle` (`renderer.c:109`) has the same unsigned-wrap weakness on the real FB.

**J.** `rendererv.c:114-117` updates `vfb_uid/vfb_base` only when `ret > 0`, so after a failed open they still name the previously freed block; `gui.c:491` tests `< 0`; `ksceKernelGetMemBlockBase` at `:116` is unchecked; `rendererv_freeVirtualFB` (`:122-124`) frees unconditionally and `gui.c:505` ignores the result. A double free is unreachable today only because all three `gui_close` callers (`main.c:174-175`, `menu.c:130`, `menu-main.c:18` via `gui_input`) are gated on `gui_isOpen`. Section 2 removes the alloc/free pair; if the allocate-once memblock alternative is used instead, apply the initialisation and guards from step 7 there. Delete the dead `SceUID mem_uid;` (`gui.c:77`).

## Build-level savings

- **`-DLOG_DEBUG` is forced by the root build.** `CMakeLists.txt:12 add_definitions(-DLOG_DEBUG)` precedes `add_subdirectory(reVita)` at `:15`, so a root build expands all 52 `LOG` sites (19 direct + 26 `HOOK_EXPORT/HOOK_IMPORT/HOOK_OFFSET` at `main.c:707-738` + 7 `IMPORT_OFFSET/IMPORT2` at `vitasdkext.c:73-80`) into the `log.h:17-38` body: a `char buffer[256]`, one `snprintf`, a `strchr` loop and five `ksceDebugPrintf` call sites, roughly 52 * 130 = 6760 B of text and rodata that the README build (`cd reVita; cmake ..`, `README.md:131-135`) compiles to `(void)0` (`log.h:55`). Nothing consumes the log (`log_reset/log_flush` have no external callers). Replace line 12 with `option(REVITA_LOG_DEBUG "verbose kernel log" OFF)` and guard the define; move the body into one out-of-line `void log_printf(const char *fmt, ...)` in `log.c` using `vsnprintf` (already linked from libk and used at `gui.c:159`, `renderer.c:102`, `rendererv.c:90`) so each site is one `bl` and the 256 B buffer lives in one frame instead of every caller's. While there, fix `ksceDebugPrintf(pchPrev)` at `log.h:31,35`, which passes user-derived text (a save path at `fio.c:28`, a titleid at `profile.c:570`) as a format string; print with `"%s"`. Two LOG sites (`sysactions.c:90`, `fio.c:28`) run in the ctrl hook's syscall context.
- **`-DRELEASE`** (`reVita/CMakeLists.txt:18`) is referenced by no source file (grep); drop it or use it to distinguish the two builds.
- **`log.c`** should be compiled only under `LOG_DISC` (savings item 5).
- **`popup.c`** in `reVita/src/gui` is not listed in `reVita/CMakeLists.txt:25-59`; it costs nothing but confuses greps. Delete or wire it in.
- **Duplicated rodata** (savings items 6 and 7) is a linkage issue, not a flag issue; `-fmerge-all-constants` would also merge them but is non-conforming and hides the bug rather than fixing it. No `-ffunction-sections/-fdata-sections --gc-sections` is present in either CMakeLists (grep); adding them is worth an experiment on the real toolchain but cannot be evaluated here.

## Checked and not a problem

- **Three full `Profile` copies (`profile`, `profile_global`, `profile_home`, `profile.h:81-87`) duplicating 24 B of constant metadata per entry.** Observation is accurate (3 * 43 * 24 = 3096 B plus padding), but the saving is under 4 KB while the fix changes the `Profile` layout exported by syscall (`revita.c:18-19`, `export.yml:13`, consumed by `reVitaMotion/src/main.c:57-59`), every `ProfileEntry*` in the menu system (`gui.h:105-106`) and the INI parser. Dropping `profile_home` would also lose unsaved HOME edits with autosave off (`profile.c:571-576`). Not worth doing.
- **`MenuEntry` shrink from 36 B to 24 B across 282 entries.** `dataInt` is indeed unused and `type` fits `int8_t` (it also holds `COMMAND_TYPE = -4`, `menu-gyro.c:9`), but the proposed `dataPE/dataPEButton` union breaks hotkey rows: `onDrawEntry_generic` (`menu.c:82-102`) tests `dataPE` before `dataPEButton`, so every hotkey entry would print a raw integer instead of the button combo. Saving is under 4 KB; only the safe subset (remove `dataInt`, `int8_t type`, `uint8_t icn`, ~2.5 KB) is worth taking if at all.
- **`rs[5][2][25]` enum and `tickPressed` int64 shrink (3250 -> ~1500 B).** Sub-page saving, and truncating the tick to 32 bits changes turbo phase after a 32-bit wrap because `isTurboTickActive` (`remap.c:254`) takes a modulo of the delta; sticky rules can be held indefinitely. Leave it.
- **`[RULE:-100000]` writing 6 MB before `profile`.** Not reachable as stated: `SECTION_ATTR_SIZE` is 2 and the key `strcpy` at `ini.c:84` truncates the attr to 2 chars + key, so only -1..-9 are reachable today (issue E still stands for those and becomes fully reachable once D is fixed).
- **`%s` of NULL when saving action 42.** Latent: nothing in the tree assigns `REMAP_REM_SWAP_TOUCHPADS` (grep: only `remap.h:73` and `profile.c:107`). The live half of issue F is the `strcmp(NULL, ...)`.
- **4096 B of "tail slack" in the remap block.** There is none: 20480 + 73728 = 94208 is page-aligned, so `MEM_SIZE` is 94208 B, not 98304 B.
- **"Only 8 buffers are ever needed" for the remap cache.** The Vita ctrl API allows up to 64 buffers per call and `main.c:211/318` bypass remapping above `BUFFERS_NUM`, so the cache depth cannot be cut without splitting the guard constant (savings item 2 does that); 16 is the safe default, not 8.
- **Coalescing the 272 per-row `ksceKernelMemcpyKernelToUser` calls into one.** Impossible: `renderer.c:206` addresses the user FB with stride `fbPitch` (960 or 512) and `:207` the VFB with stride `uiWidth` (480); rows are contiguous in neither buffer.
- **Dirty-frame tracking as a RAM fix.** Saves 0 B (CPU only), and several menus poll live state in `onDraw`; noted under section 3 as optional.
- **Band renderer (static 480xB band, redraw the scene 8-17 times per frame).** Fully removes the allocation but multiplies every `vsnprintf` and every side-effecting `onDraw` (`menu-debug-buttons.c:28`, `menu-controller.c:30`) per frame inside the game's flip syscall for the same ~65 KB result the 4bpp buffer reaches with a localised change. Rejected by all three judges.
- **CDRAM (`0x40408006`/`0x40404006`) as the primary VFB pool.** Moves the 512 KB into the game's 112 MiB VRAM budget per open rather than removing it, is 256 KiB-granular (so it cannot benefit from the 4bpp shrink), is uncached in the 0x40408006 variant, and its availability from a plugin thread mid-game is inferred from other plugins, not verified. Acceptable only as an optional last rung.
- **`KERNEL_ROOT_PHYCONT_RW` (0x1080D006) as a fallback.** Demands physically contiguous pages; strictly harder to satisfy than today's request. Do not add.
- **Direct-to-framebuffer fallback for the whole menu when the alloc fails.** API-valid (popups already do it) but the `drawPixel` path is one kernel-to-user copy per set pixel, on the order of 10^5 calls per frame, and the sketch that set `gui_isOpen = true; vfb_base = NULL` would crash in `renderer_writeFromVFB` (`renderer.c:207`). Superseded by removing the allocation.
- **`static const` copies of `ICON[]` pulled into ~25 TUs via `gui.h:4`.** Only the two indexing TUs emit a copy; unreferenced `static const` objects are dropped at `-O3`, so the other ~23 cost compile time, not bytes.
- **16bpp RGB565 VFB.** Half the saving of 8bpp, loses 2-3 bits of user-editable theme colours (`theme.c:118-131`), still needs per-row conversion. Not worth it next to 4bpp/8bpp.

## Suggested order of work

1. Crash fixes that cost nothing and are one-liners: A (touch report bound, `remap.c:138-190`), F (NULL guard + 43rd string, `profile.c:65-114`), E (`ruleId < 0` and `parseInt` init, `profile.c:428-433`, `ini.c:127-133`), H (`fio_copyFile` labels, `fio.c:112-150`), J/dead `mem_uid` (`gui.c:77`). Apply each to `reVitaHeadless/src` too where the file exists there.
2. The INI trio together, since each alone converts a crash into silent data loss: B (`ini_append` bounded, capacity in `struct INI`, 12288 B profile buffer, real sizes at every `ini_create`), C (`fio_readFile` reads `size - 1`, NUL-terminates, readers `memset`), D (`sscanf` widths, `SECTION_ATTR_SIZE 10`, `listVal` clamp). Add a popup on `profile_save` failure in `gui_close`.
3. The popup fix (section 2): static 4bpp indexed VFB, clipped `vput`, palette expansion in `renderer_writeFromVFB`, delete `gui.c:491-495` and `:505`, add `rendererv_destroy()` to `gui_destroy`, fix `menu-debug-buttons.c:32/35/47`. Verify on hardware with ucdc + PSVShell, animation on and off, no-flicker on, PS TV, Adrenaline, the pick-touch menus, odd-x drawIndent and a 40+ char right-aligned value.
4. Free static wins in the same release so the module image shrinks by more than the new .bss grows: drop `log_buf` (`log.c:8`), deduplicate `font[]`/`ICON[]` into `gui/img/img.c`, build option for `LOG_DEBUG` with an out-of-line `log_printf`. Net static change after step 3: 67264 - 34304 = 32960 B.
5. Remap cache: split `BUFFERS_NUM` into `MAX_HOOK_BUFS 64` and `CACHE_BUFS 16`, `TOUCH_CACHE_HOOKS_NUM 2` (including the reset loop at `remap.c:1178-1181`), static .bss arrays, `memmove` shift, fast-path `num > 0` guard, clamp-after-newest-sample in `main.c`. This removes issue G and 94208 - 14336 = 79872 B of permanent kernel RAM. Mirror in the headless tree.
6. Save backup/restore buffer: allocate once per tree at 32768 B (`fio.c:8,110`), fix the swapped BACKUP/RESTORE calls at `remap.c:387-388` while there.
7. Emulated-touch pass-by-pointer and struct repack (`gui.c:355-358`, `remap.h:132-141`); optional `rendererv_setColorId` conversion of the 34 call sites; optional row-buffered `renderer_drawImage` and dirty flag for CPU.
8. Documentation: rewrite `README.md:123` (the popup no longer exists after step 3), note the `-DREVITA_LOG_DEBUG` option, and state that reVita must be rebuilt from both `reVita/` and `reVitaHeadless/` after the shared `fio/` and `remap.c` changes.