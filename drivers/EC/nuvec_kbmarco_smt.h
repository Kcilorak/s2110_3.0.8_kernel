#ifndef KB_MARCO_H
#define KB_MARCO_H
/*-----------------------------------------------------------------------------
 * Macros  for kbd  1108 weihuai
 *---------------------------------------------------------------------------*/

/* Keyboard special scancode */
#define KEY_ESC_E0      0xE0
#define KEY_ESC_E1      0xE1
#define KEY_BREAK_F0    0xF0

/* Keyboard keycodes */
#define NOKEY           KEY_RESERVED
#define KEY_LEFTWIN     KEY_LEFTMETA
#define KEY_RIGHTWIN    KEY_RIGHTMETA
#define KEY_APPS        KEY_COMPOSE
#define KEY_PRINTSCR    KEY_SYSRQ

#define NTC_KEY_F1      NOKEY
#define NTC_KEY_F2      NOKEY
#define NTC_KEY_F3      NOKEY
#define NTC_KEY_F4      NOKEY
#define NTC_KEY_F5      NOKEY
#define NTC_KEY_F6      NOKEY
#define NTC_KEY_F7      NOKEY
#define NTC_KEY_F8      NOKEY
#define NTC_KEY_F9      NOKEY
#define NTC_KEY_F10     NOKEY
#define NTC_KEY_F11     NOKEY
#define NTC_KEY_F12     NOKEY
#define NTC_KEY_1       NOKEY


/*-----------------------------------------------------------------------------
 * Keyboard scancode to linux keycode translation table
 *---------------------------------------------------------------------------*/
static /*const*/ unsigned char set1_keycode[512] = {
/* 00h */ NOKEY,          KEY_ESC,    KEY_1,         KEY_2,          KEY_3,     KEY_4,       KEY_5,          KEY_6,
/* 08h */ KEY_7,          KEY_8,      KEY_9,         KEY_0,          KEY_MINUS, KEY_EQUAL,   KEY_BACKSPACE,  KEY_TAB,
/* 10h */ KEY_Q,          KEY_W,      KEY_E,         KEY_R,          KEY_T,     KEY_Y,       KEY_U,          KEY_I,
/* 18h */ KEY_O,          KEY_P,      KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL,KEY_A,          KEY_S,
/* 20h */ KEY_D,          KEY_F,      KEY_G,         KEY_H,          KEY_J,     KEY_K,       KEY_L,          KEY_SEMICOLON,
/* 28h */ KEY_APOSTROPHE, KEY_GRAVE,  KEY_LEFTSHIFT, KEY_BACKSLASH,  KEY_Z,     KEY_X,       KEY_C,          KEY_V,
/* 30h */ KEY_B,          KEY_N,      KEY_M,         KEY_COMMA,      KEY_DOT,   KEY_SLASH,   KEY_RIGHTSHIFT, KEY_KPSLASH,
/* 38h */ KEY_LEFTALT,    KEY_SPACE,  KEY_CAPSLOCK,  KEY_SEARCH,    KEY_WLAN  ,    KEY_F3,      KEY_BLUETOOTH,         KEY_F17/*KEY_F5*/,
/* 40h */ KEY_F22,      KEY_F7 ,   KEY_BRIGHTNESSUP ,       KEY_MUTE,        KEY_VOLUMEDOWN ,   KEY_NUMLOCK, KEY_F24, KEY_7,
/* 48h */ KEY_8,       KEY_9,   KEY_KPASTERISK,   KEY_4,    KEY_5,  KEY_6 ,    KEY_KPMINUS ,    KEY_1,
/* 50h */ KEY_2,        KEY_3,   KEY_0,     KEY_DOT  ,      NOKEY,     NOKEY,       NOKEY,       KEY_VOLUMEUP ,
/* 58h */ KEY_PREVIOUSSONG,        NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* 60h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* 68h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* 70h */ NOKEY,          NOKEY,      NOKEY,         KEY_F20,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* 78h */ /*SMT-start*/KEY_KP1,       KEY_KP2,   KEY_KP3,      KEY_KP4,       KEY_KP5,/*SMT-end*/    NOKEY,       NOKEY,          NOKEY,
/* 80h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* 88h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* 90h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* 98h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* A0h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* A8h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* B0h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* B8h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* C0h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* C8h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* D0h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* D8h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* E0h */ KEY_ESC_E0,     KEY_ESC_E1, NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* E8h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* F0h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,
/* F8h */ NOKEY,          NOKEY,      NOKEY,         NOKEY,          NOKEY,     NOKEY,       NOKEY,          NOKEY,

    /*-----------------------------------------------------------------------------
     * Start of escape code (E0, E1) scan codes
     *---------------------------------------------------------------------------*/

/* 00h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 08h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 10h */ KEY_F17/*KEY_F5*/,   NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 18h */ NOKEY,      KEY_F22  ,   NOKEY,      NOKEY,       NOKEY,        KEY_RIGHTCTRL, NOKEY,       NOKEY,       
/* 20h */ KEY_VOLUMEDOWN,  NTC_KEY_1,    KEY_BRIGHTNESSUP, NOKEY,       KEY_MUTE,   NOKEY,         NOKEY,       NOKEY,
/* 28h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,        KEY_VOLUMEUP , NOKEY,
/* 30h */ KEY_PREVIOUSSONG,  NOKEY,        KEY_WLAN, NOKEY,       NOKEY,      KEY_KPPLUS  ,   NOKEY,       KEY_NEXTSONG,       
/* 38h */ KEY_RIGHTALT, NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 40h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        KEY_PLAYPAUSE,     NOKEY,       KEY_HOME,       
/* 48h */ KEY_UP,       KEY_PAGEUP,   NOKEY,      KEY_LEFT,    NOKEY,    KEY_RIGHT,     NOKEY,  KEY_MENU,       
/* 50h */ KEY_DOWN,     KEY_PAGEDOWN, KEY_F18/*KEY_PRINT*/, KEY_F2/*KEY_DELETE*/,  NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 58h */ NOKEY,        NOKEY,        NOKEY,     KEY_F13/*KEY_F23*/, KEY_RIGHTWIN, KEY_F14/*KEY_UNKNOWN*/,      NOKEY,     KEY_SEARCH ,
/* 60h */ NOKEY,        NOKEY,        NOKEY,      KEY_F7,  NOKEY,        NOKEY,         NOKEY,       KEY_BLUETOOTH,       
/* 68h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       KEY_F3,   NOKEY,         NOKEY,       NOKEY,       
/* 70h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 78h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 80h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 88h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 90h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* 98h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* A0h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* A8h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* B0h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* B8h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* C0h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* C8h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* D0h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* D8h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* E0h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* E8h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* F0h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         NOKEY,       NOKEY,       
/* F8h */ NOKEY,        NOKEY,        NOKEY,      NOKEY,       NOKEY,        NOKEY,         KEY_F15,       KEY_F16,       
};
/*-----------------------------------------------------------------------------
 * Macros
 *---------------------------------------------------------------------------*/
#endif
