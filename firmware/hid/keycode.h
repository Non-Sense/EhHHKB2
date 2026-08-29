#ifndef EHHHKB2_KEYCODE_H
#define EHHHKB2_KEYCODE_H

#include <stdint.h>

#include "class/hid/hid.h"

// clang-format off
#define KC_NONE                       HID_KEY_NONE
#define KC_TRANS                      0x01
#define KC_FN                         0xf0
#define KC_FN2                        0xf1

// 16bit 仮想キーコードの割り当て: 0x0000-0x00FF=HID キーボード usage（キーボードレポートへ入れる）、0x0100-0x0FFF=ファームウェア内部のアクション（HID へは出さない）、0x1000-0x1FFF=Consumer Control usage（メディアキー。専用レポートで送る）
#define KC_ACTION_BASE                ((uint16_t)0x0100)
#define KC_BT1                        (KC_ACTION_BASE + 0x00)
#define KC_BT2                        (KC_ACTION_BASE + 0x01)
#define KC_BT3                        (KC_ACTION_BASE + 0x02)
#define KC_BT4                        (KC_ACTION_BASE + 0x03)
#define KC_BT5                        (KC_ACTION_BASE + 0x04)
#define KC_BT6                        (KC_ACTION_BASE + 0x05)
#define KC_PAIR                       (KC_ACTION_BASE + 0x10)
#define KC_BRST                       (KC_ACTION_BASE + 0x11)
#define KC_USB                        (KC_ACTION_BASE + 0x12)
#define KC_CFG                        (KC_ACTION_BASE + 0x13)

// Consumer Control（Usage Page 0x0C）。下位 12bit が usage そのもの。キーボードページの KC_MUTE / KC_VOLUME_UP / KC_VOLUME_DOWN とは別物で、メディアキーとして使えるのはこちら。
#define KC_MEDIA_BASE                 ((uint16_t)0x1000)
#define KC_MPLAY                      (KC_MEDIA_BASE + 0x00CD)
#define KC_MSTOP                      (KC_MEDIA_BASE + 0x00B7)
#define KC_MPREV                      (KC_MEDIA_BASE + 0x00B6)
#define KC_MNEXT                      (KC_MEDIA_BASE + 0x00B5)
#define KC_MMUTE                      (KC_MEDIA_BASE + 0x00E2)
#define KC_MVOLU                      (KC_MEDIA_BASE + 0x00E9)
#define KC_MVOLD                      (KC_MEDIA_BASE + 0x00EA)

#define KC_IS_ACTION(kc)              ((kc) >= KC_ACTION_BASE && \
                                       (kc) < KC_MEDIA_BASE)
#define KC_IS_MEDIA(kc)               ((kc) >= KC_MEDIA_BASE)
#define KC_MEDIA_USAGE(kc)            ((uint16_t)((kc) - KC_MEDIA_BASE))

// 真なら KC_MOD_BIT() で modifier ビットへ変換できる（0xE0..0xE7）
#define KC_IS_MODIFIER(kc)            ((kc) >= KC_LCTL && (kc) <= KC_RGUI)
#define KC_MOD_BIT(kc)                ((uint8_t)(1u << ((kc) - KC_LCTL)))

#define KC_A                          HID_KEY_A
#define KC_B                          HID_KEY_B
#define KC_C                          HID_KEY_C
#define KC_D                          HID_KEY_D
#define KC_E                          HID_KEY_E
#define KC_F                          HID_KEY_F
#define KC_G                          HID_KEY_G
#define KC_H                          HID_KEY_H
#define KC_I                          HID_KEY_I
#define KC_J                          HID_KEY_J
#define KC_K                          HID_KEY_K
#define KC_L                          HID_KEY_L
#define KC_M                          HID_KEY_M
#define KC_N                          HID_KEY_N
#define KC_O                          HID_KEY_O
#define KC_P                          HID_KEY_P
#define KC_Q                          HID_KEY_Q
#define KC_R                          HID_KEY_R
#define KC_S                          HID_KEY_S
#define KC_T                          HID_KEY_T
#define KC_U                          HID_KEY_U
#define KC_V                          HID_KEY_V
#define KC_W                          HID_KEY_W
#define KC_X                          HID_KEY_X
#define KC_Y                          HID_KEY_Y
#define KC_Z                          HID_KEY_Z
#define KC_1                          HID_KEY_1
#define KC_2                          HID_KEY_2
#define KC_3                          HID_KEY_3
#define KC_4                          HID_KEY_4
#define KC_5                          HID_KEY_5
#define KC_6                          HID_KEY_6
#define KC_7                          HID_KEY_7
#define KC_8                          HID_KEY_8
#define KC_9                          HID_KEY_9
#define KC_0                          HID_KEY_0
#define KC_ENTER                      HID_KEY_ENTER
#define KC_ESC                        HID_KEY_ESCAPE
#define KC_BSPC                       HID_KEY_BACKSPACE
#define KC_TAB                        HID_KEY_TAB
#define KC_SPACE                      HID_KEY_SPACE
#define KC_MIN                        HID_KEY_MINUS
#define KC_EQL                        HID_KEY_EQUAL
#define KC_LBRC                       HID_KEY_BRACKET_LEFT
#define KC_RBRC                       HID_KEY_BRACKET_RIGHT
#define KC_BSLS                       HID_KEY_BACKSLASH
#define KC_EUROPE_1                   HID_KEY_EUROPE_1
#define KC_SCLN                       HID_KEY_SEMICOLON
#define KC_QUOT                       HID_KEY_APOSTROPHE
#define KC_GRV                        HID_KEY_GRAVE
#define KC_COMM                       HID_KEY_COMMA
#define KC_DOT                        HID_KEY_PERIOD
#define KC_SLSH                       HID_KEY_SLASH
#define KC_CAPS                       HID_KEY_CAPS_LOCK
#define KC_F1                         HID_KEY_F1
#define KC_F2                         HID_KEY_F2
#define KC_F3                         HID_KEY_F3
#define KC_F4                         HID_KEY_F4
#define KC_F5                         HID_KEY_F5
#define KC_F6                         HID_KEY_F6
#define KC_F7                         HID_KEY_F7
#define KC_F8                         HID_KEY_F8
#define KC_F9                         HID_KEY_F9
#define KC_F10                        HID_KEY_F10
#define KC_F11                        HID_KEY_F11
#define KC_F12                        HID_KEY_F12
#define KC_PSCR                       HID_KEY_PRINT_SCREEN
#define KC_SCLK                       HID_KEY_SCROLL_LOCK
#define KC_PAUSE                      HID_KEY_PAUSE
#define KC_INS                        HID_KEY_INSERT
#define KC_HOME                       HID_KEY_HOME
#define KC_PGUP                       HID_KEY_PAGE_UP
#define KC_DEL                        HID_KEY_DELETE
#define KC_END                        HID_KEY_END
#define KC_PGDN                       HID_KEY_PAGE_DOWN
#define KC_RIGHT                      HID_KEY_ARROW_RIGHT
#define KC_LEFT                       HID_KEY_ARROW_LEFT
#define KC_DOWN                       HID_KEY_ARROW_DOWN
#define KC_UP                         HID_KEY_ARROW_UP
#define KC_NUM_LOCK                   HID_KEY_NUM_LOCK
#define KC_KEYPAD_DIVIDE              HID_KEY_KEYPAD_DIVIDE
#define KC_KEYPAD_MULTIPLY            HID_KEY_KEYPAD_MULTIPLY
#define KC_KEYPAD_SUBTRACT            HID_KEY_KEYPAD_SUBTRACT
#define KC_KEYPAD_ADD                 HID_KEY_KEYPAD_ADD
#define KC_KEYPAD_ENTER               HID_KEY_KEYPAD_ENTER
#define KC_KEYPAD_1                   HID_KEY_KEYPAD_1
#define KC_KEYPAD_2                   HID_KEY_KEYPAD_2
#define KC_KEYPAD_3                   HID_KEY_KEYPAD_3
#define KC_KEYPAD_4                   HID_KEY_KEYPAD_4
#define KC_KEYPAD_5                   HID_KEY_KEYPAD_5
#define KC_KEYPAD_6                   HID_KEY_KEYPAD_6
#define KC_KEYPAD_7                   HID_KEY_KEYPAD_7
#define KC_KEYPAD_8                   HID_KEY_KEYPAD_8
#define KC_KEYPAD_9                   HID_KEY_KEYPAD_9
#define KC_KEYPAD_0                   HID_KEY_KEYPAD_0
#define KC_KEYPAD_DECIMAL             HID_KEY_KEYPAD_DECIMAL
#define KC_EUROPE_2                   HID_KEY_EUROPE_2
#define KC_APPLICATION                HID_KEY_APPLICATION
#define KC_POWER                      HID_KEY_POWER
#define KC_KEYPAD_EQUAL               HID_KEY_KEYPAD_EQUAL
#define KC_F13                        HID_KEY_F13
#define KC_F14                        HID_KEY_F14
#define KC_F15                        HID_KEY_F15
#define KC_F16                        HID_KEY_F16
#define KC_F17                        HID_KEY_F17
#define KC_F18                        HID_KEY_F18
#define KC_F19                        HID_KEY_F19
#define KC_F20                        HID_KEY_F20
#define KC_F21                        HID_KEY_F21
#define KC_F22                        HID_KEY_F22
#define KC_F23                        HID_KEY_F23
#define KC_F24                        HID_KEY_F24
#define KC_EXECUTE                    HID_KEY_EXECUTE
#define KC_HELP                       HID_KEY_HELP
#define KC_MENU                       HID_KEY_MENU
#define KC_SELECT                     HID_KEY_SELECT
#define KC_STOP                       HID_KEY_STOP
#define KC_AGAIN                      HID_KEY_AGAIN
#define KC_UNDO                       HID_KEY_UNDO
#define KC_CUT                        HID_KEY_CUT
#define KC_COPY                       HID_KEY_COPY
#define KC_PASTE                      HID_KEY_PASTE
#define KC_FIND                       HID_KEY_FIND
#define KC_MUTE                       HID_KEY_MUTE
#define KC_VOLUME_UP                  HID_KEY_VOLUME_UP
#define KC_VOLUME_DOWN                HID_KEY_VOLUME_DOWN
#define KC_LOCKING_CAPS_LOCK          HID_KEY_LOCKING_CAPS_LOCK
#define KC_LOCKING_NUM_LOCK           HID_KEY_LOCKING_NUM_LOCK
#define KC_LOCKING_SCROLL_LOCK        HID_KEY_LOCKING_SCROLL_LOCK
#define KC_KEYPAD_COMMA               HID_KEY_KEYPAD_COMMA
#define KC_KEYPAD_EQUAL_SIGN          HID_KEY_KEYPAD_EQUAL_SIGN
#define KC_KANJI1                     HID_KEY_KANJI1
#define KC_KANJI2                     HID_KEY_KANJI2
#define KC_KANJI3                     HID_KEY_KANJI3
#define KC_KANJI4                     HID_KEY_KANJI4
#define KC_KANJI5                     HID_KEY_KANJI5
#define KC_KANJI6                     HID_KEY_KANJI6
#define KC_KANJI7                     HID_KEY_KANJI7
#define KC_KANJI8                     HID_KEY_KANJI8
#define KC_KANJI9                     HID_KEY_KANJI9
#define KC_LANG1                      HID_KEY_LANG1
#define KC_LANG2                      HID_KEY_LANG2
#define KC_LANG3                      HID_KEY_LANG3
#define KC_LANG4                      HID_KEY_LANG4
#define KC_LANG5                      HID_KEY_LANG5
#define KC_LANG6                      HID_KEY_LANG6
#define KC_LANG7                      HID_KEY_LANG7
#define KC_LANG8                      HID_KEY_LANG8
#define KC_LANG9                      HID_KEY_LANG9
#define KC_ALTERNATE_ERASE            HID_KEY_ALTERNATE_ERASE
#define KC_SYSREQ_ATTENTION           HID_KEY_SYSREQ_ATTENTION
#define KC_CANCEL                     HID_KEY_CANCEL
#define KC_CLEAR                      HID_KEY_CLEAR
#define KC_PRIOR                      HID_KEY_PRIOR
#define KC_RETURN                     HID_KEY_RETURN
#define KC_SEPARATOR                  HID_KEY_SEPARATOR
#define KC_OUT                        HID_KEY_OUT
#define KC_OPER                       HID_KEY_OPER
#define KC_CLEAR_AGAIN                HID_KEY_CLEAR_AGAIN
#define KC_CRSEL_PROPS                HID_KEY_CRSEL_PROPS
#define KC_EXSEL                      HID_KEY_EXSEL
#define KC_KEYPAD_00                  HID_KEY_KEYPAD_00
#define KC_KEYPAD_000                 HID_KEY_KEYPAD_000
#define KC_THOUSANDS_SEPARATOR        HID_KEY_THOUSANDS_SEPARATOR
#define KC_DECIMAL_SEPARATOR          HID_KEY_DECIMAL_SEPARATOR
#define KC_CURRENCY_UNIT              HID_KEY_CURRENCY_UNIT
#define KC_CURRENCY_SUBUNIT           HID_KEY_CURRENCY_SUBUNIT
#define KC_KEYPAD_LEFT_PARENTHESIS    HID_KEY_KEYPAD_LEFT_PARENTHESIS
#define KC_KEYPAD_RIGHT_PARENTHESIS   HID_KEY_KEYPAD_RIGHT_PARENTHESIS
#define KC_KEYPAD_LEFT_BRACE          HID_KEY_KEYPAD_LEFT_BRACE
#define KC_KEYPAD_RIGHT_BRACE         HID_KEY_KEYPAD_RIGHT_BRACE
#define KC_KEYPAD_TAB                 HID_KEY_KEYPAD_TAB
#define KC_KEYPAD_BACKSPACE           HID_KEY_KEYPAD_BACKSPACE
#define KC_KEYPAD_A                   HID_KEY_KEYPAD_A
#define KC_KEYPAD_B                   HID_KEY_KEYPAD_B
#define KC_KEYPAD_C                   HID_KEY_KEYPAD_C
#define KC_KEYPAD_D                   HID_KEY_KEYPAD_D
#define KC_KEYPAD_E                   HID_KEY_KEYPAD_E
#define KC_KEYPAD_F                   HID_KEY_KEYPAD_F
#define KC_KEYPAD_XOR                 HID_KEY_KEYPAD_XOR
#define KC_KEYPAD_CARET               HID_KEY_KEYPAD_CARET
#define KC_KEYPAD_PERCENT             HID_KEY_KEYPAD_PERCENT
#define KC_KEYPAD_LESS_THAN           HID_KEY_KEYPAD_LESS_THAN
#define KC_KEYPAD_GREATER_THAN        HID_KEY_KEYPAD_GREATER_THAN
#define KC_KEYPAD_AMPERSAND           HID_KEY_KEYPAD_AMPERSAND
#define KC_KEYPAD_DOUBLE_AMPERSAND    HID_KEY_KEYPAD_DOUBLE_AMPERSAND
#define KC_KEYPAD_VERTICAL_BAR        HID_KEY_KEYPAD_VERTICAL_BAR
#define KC_KEYPAD_DOUBLE_VERTICAL_BAR HID_KEY_KEYPAD_DOUBLE_VERTICAL_BAR
#define KC_KEYPAD_COLON               HID_KEY_KEYPAD_COLON
#define KC_KEYPAD_HASH                HID_KEY_KEYPAD_HASH
#define KC_KEYPAD_SPACE               HID_KEY_KEYPAD_SPACE
#define KC_KEYPAD_AT                  HID_KEY_KEYPAD_AT
#define KC_KEYPAD_EXCLAMATION         HID_KEY_KEYPAD_EXCLAMATION
#define KC_KEYPAD_MEMORY_STORE        HID_KEY_KEYPAD_MEMORY_STORE
#define KC_KEYPAD_MEMORY_RECALL       HID_KEY_KEYPAD_MEMORY_RECALL
#define KC_KEYPAD_MEMORY_CLEAR        HID_KEY_KEYPAD_MEMORY_CLEAR
#define KC_KEYPAD_MEMORY_ADD          HID_KEY_KEYPAD_MEMORY_ADD
#define KC_KEYPAD_MEMORY_SUBTRACT     HID_KEY_KEYPAD_MEMORY_SUBTRACT
#define KC_KEYPAD_MEMORY_MULTIPLY     HID_KEY_KEYPAD_MEMORY_MULTIPLY
#define KC_KEYPAD_MEMORY_DIVIDE       HID_KEY_KEYPAD_MEMORY_DIVIDE
#define KC_KEYPAD_PLUS_MINUS          HID_KEY_KEYPAD_PLUS_MINUS
#define KC_KEYPAD_CLEAR               HID_KEY_KEYPAD_CLEAR
#define KC_KEYPAD_CLEAR_ENTRY         HID_KEY_KEYPAD_CLEAR_ENTRY
#define KC_KEYPAD_BINARY              HID_KEY_KEYPAD_BINARY
#define KC_KEYPAD_OCTAL               HID_KEY_KEYPAD_OCTAL
#define KC_KEYPAD_DECIMAL_2           HID_KEY_KEYPAD_DECIMAL_2
#define KC_KEYPAD_HEXADECIMAL         HID_KEY_KEYPAD_HEXADECIMAL
#define KC_LCTL                       HID_KEY_CONTROL_LEFT
#define KC_LSFT                       HID_KEY_SHIFT_LEFT
#define KC_LALT                       HID_KEY_ALT_LEFT
#define KC_LGUI                       HID_KEY_GUI_LEFT
#define KC_RCTL                       HID_KEY_CONTROL_RIGHT
#define KC_RSFT                       HID_KEY_SHIFT_RIGHT
#define KC_RALT                       HID_KEY_ALT_RIGHT
#define KC_RGUI                       HID_KEY_GUI_RIGHT
// clang-format on

#endif
