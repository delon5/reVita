#ifndef _ICONS_FONT_H_
#define _ICONS_FONT_H_

#define ICON_W 24
#define ICON_H 20
#define ICON_BYTES (ICON_W * ICON_H / 8)   // 60 B per icon

typedef enum {
	ICON_NULL = 0,
	NOI_1,
	NOI_2,
	NOI_3,
	NOI_4,
	NOI_5,
	NOI_6,
	NOI_7,
	NOI_8,
	NOI_9,
	NOI_10,
	NOI_11,
	NOI_12,
	NOI_13,
	NOI_14,
	NOI_15,
	NOI_16,
	NOI_17,
	NOI_18,
	NOI_19,
	NOI_20,
	NOI_21,
	NOI_22,
	NOI_23,
	NOI_24,
	NOI_25,
	NOI_26,
	NOI_27,
	NOI_28,
	NOI_29,
	NOI_30,
	NOI_31,
	NOI_SPACE,			// " "
	ICON_DANGER,		// "!"
	NOI_34,				// """
	ICON_OFF_R,			// "#"
	NOI_36,				// "$"
	ICON_DISABLED,		// "%"
	NOI_38,				// "&"
	NOI_39,				// "'"
	ICON_BTN_L3,		// "("
	ICON_BTN_R3,		// ")"
	NOI_42,				// "*"
	ICON_BTN_VOLUP,		// "+"
	ICON_BTN_L2,		// ","
	ICON_BTN_VOLDOWN,	// "-"
	ICON_BTN_R2,		// "."
	ICON_MENU_STORAGE,	// "/"
	NOI_48,				// "0"
	ICON_L,				// "1"
	ICON_R,				// "2"
	ICON_TL,			// "3"
	ICON_TR,			// "4"
	ICON_BL,			// "5"
	ICON_BR,			// "6"
	ICON_CENTER,		// "7"
	ICON_FULL,			// "8"
	NOI_57,				// "9"
	ICON_BTN_START,		// ":"
	ICON_BTN_SELECT,	// ";"
	ICON_BTN_LEFT,		// "<"
	NOI_61,				// "="
	ICON_BTN_RIGHT,		// ">"
	ICON_MENU_CREDITS,	// "?"
	ICON_ON_L,			// "@"
	NOI_65,				// "A"
	ICON_BT,			// "B"
	ICON_BTN_CIRCLE,	// "C"
	ICON_LS_DOWN,		// "D"
	ICON_GY_ROLLRIGHT,	// "E"
	ICON_FT,			// "F"
	ICON_SAVE,			// "G"
	ICON_LOAD,			// "H"
	ICON_POPUP,			// "I"
	ICON_DELETE,		// "J"
	ICON_TURBO_SLOW,	// "K"
	ICON_LS_LEFT,		// "L"
	ICON_TURBO_MED,		// "M"
	ICON_TURBO_FAST,	// "N"
	ICON_PSV_RIGHT,		// "O"
	ICON_BTN_PS,		// "P"
	ICON_GY_ROLLLEFT,	// "Q"
	ICON_LS_RIGHT,		// "R"
	ICON_BTN_SQUARE,	// "S"
	ICON_BTN_TRIANGLE,	// "T"
	ICON_LS_UP,			// "U"
	ICON_PSTV,			// "V"
	NOI_87,				// "W"
	ICON_BTN_CROSS,		// "X"
	ICON_WHEEL,			// "Y"
	ICON_CROSSHAIR,		// "Z"
	ICON_BTN_LT,		// "["
	NOI_92,				// "A"
	ICON_BTN_RT,		// "]"
	ICON_BTN_UP,		// "^"
	NOI_95,				// "_"
	ICON_ON_R,			// "'"
	NOI_97,				// "a"
	ICON_MENU_BUG,		// "b"
	ICON_CONFIG,		// "c"
	ICON_RS_DOWN,		// "d"
	ICON_GY_RIGHT,		// "e"
	NOI_102,			// "f"
	NOI_103,			// "g"
	ICON_HEADPHONES,	// "h"
	ICON_TOUCH,			// "i"
	ICON_SWIPE,			// "j"
	NOI_107,			// "k"
	ICON_RS_LEFT,		// "l"
	ICON_STICKY,		// "m"
	NOI_110,			// "n"
	ICON_PSV_LEFT,		// "o"
	ICON_BTN_POWER,		// "p"
	ICON_GY_LEFT,		// "q"
	ICON_RS_RIGHT,		// "r"
	ICON_GY_DOWN,		// "s"
	ICON_BTN_DS4TOUCH,	// "t"
	ICON_RS_UP,			// "u"
	ICON_BTN_DONW,		// "v"
	ICON_GY_UP,			// "w"
	ICON_DPAD,			// "x"
	ICON_MIC,			// "y"
	NOI_122,			// "z"
	ICON_BTN_L1,		// "{"
	ICON_MENU_SETTINGS,	// "|"
	ICON_BTN_R1,		// "}"
	ICON_OFF_L,			// "~"
	NOI_127,			//
	ICON_ID__NUM
}ICON_ID;
// Defined once in icons-font-data.c
extern const unsigned char ICON[ICON_ID__NUM * ICON_BYTES];

#endif
