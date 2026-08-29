#include "hid/key_matrix.h"

#include <string.h>

#include "class/hid/hid.h"
#include "keycode.h"
#include "pico/stdlib.h"

#define KEY_DEBOUNCE_TICKS 4

static layer_t current_layer = _BASE;

#define ____ KC_TRANS
#define _____ KC_TRANS
#define ______ KC_TRANS
#define _______ KC_TRANS

// clang-format off
const uint16_t keymap[LAYER_NUM][KEY_MATRIX_ROWS][KEY_MATRIX_COLS] = {
    [_BASE] = LAYOUT(
                  KC_F1,   KC_F2,   KC_F3,   KC_F4,    KC_F5,   KC_F6,   KC_F7,   KC_F8,     KC_F9,   KC_F10,   KC_F11,  KC_F12,                                  \
        KC_ESC,  KC_1,    KC_2,    KC_3,    KC_4,    KC_5,    KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MIN,  KC_EQL,  KC_BSLS, KC_GRV,   KC_HOME, KC_END,  \
         KC_TAB,  KC_Q,    KC_W,    KC_E,    KC_R,    KC_T,    KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_LBRC, KC_RBRC,     KC_BSPC,      KC_DEL,  KC_INS,  \
          KC_LCTL, KC_A,    KC_S,    KC_D,    KC_F,    KC_G,    KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, KC_QUOT,          KC_ENTER,       KC_PSCR, KC_PGUP, \
          KC_LSFT,  KC_Z,    KC_X,    KC_C,    KC_V,    KC_B,    KC_N,    KC_M,    KC_COMM, KC_DOT,  KC_SLSH,            KC_RSFT,      KC_FN,   KC_UP,   KC_PGDN, \
        KC_LCTL,  KC_FN,  KC_LALT,                    KC_SPACE,                            KC_RGUI,  KC_RALT,   KC_RCTL,               KC_LEFT, KC_DOWN, KC_RIGHT
    ),
    [_FN] = LAYOUT(
                  _____,   _____,   _____,   _____,    _____,   _____,   _____,   _____,     _____,   ______,   ______,  ______,                                  \
        KC_FN2,  KC_F1,  KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,   KC_F7,   KC_F8,   KC_F9,  KC_F10,    KC_F11,  KC_F12,   KC_KANJI3, KC_KANJI1,   _______,  ______, \
         KC_CAPS, ____,    ____,    ____,    ____,    ____,    ____,    ____,   KC_PSCR, KC_SCLK, KC_PAUSE, KC_UP,   _______,     _______,      _______,  ______, \
          _______, ____,    ____,    ____,    ____,    ____,    ____,    ____,    KC_HOME, KC_PGUP, KC_LEFT, KC_RIGHT,          KC_KEYPAD_ENTER, _______,  KC_MVOLU, \
          _______,  ____,    ____,    ____,    ____,    ____,    ____,    ____,    KC_END, KC_PGDN,  KC_DOWN,            _______,      _____,   KC_MSTOP, KC_MVOLD, \
        _______,  _____,  _______,                     _______,                            _______,  _______,   _______,              KC_MPREV, KC_MPLAY, KC_MNEXT
    ),
    [_FN2] = LAYOUT(
                  _____,   _____,   _____,   _____,    _____,   _____,   _____,   _____,     _____,   ______,   ______,  ______,                                  \
        KC_FN2,  KC_BT1,  KC_BT2,  KC_BT3,  KC_BT4,  KC_BT5,  KC_BT6,  KC_USB,  ____,    ____,    ____,    ______,  ______,  _______, ______,   _______, KC_CFG,  \
         ______,  ____,    ____,    ____,    ____,    ____,    ____,    ____,    ____,    ____,    ____,    _______, _______,     _______,      ______,  ______,  \
          _______, ____,    ____,    ____,    ____,    ____,    ____,    ____,    ____,    ____,    _______, _______,           _______,        _______, _______, \
          _______,  KC_BRST,    ____,    ____,    ____,    ____,    ____,    ____,    _______, ______,  _______,            _______,      _____,   _____,   _______, \
        _______,  _____,  _______,                     KC_PAIR,                            _______,  _______,   _______,               _______, _______, _______
    ),
};
// clang-format on

void key_matrix_init(key_matrix_t *matrix, const uint8_t *row_pins,
                     const uint8_t *col_pins) {
    memcpy(matrix->row_pins, row_pins, KEY_MATRIX_ROWS);
    memcpy(matrix->col_pins, col_pins, KEY_MATRIX_COLS);

    // row: 入力（プルアップ）、col: 出力（通常 HIGH、スキャン時に 1 本ずつ LOW）
    for (uint8_t i = 0; i < KEY_MATRIX_ROWS; ++i) {
        gpio_init(matrix->row_pins[i]);
        gpio_set_dir(matrix->row_pins[i], GPIO_IN);
        gpio_set_pulls(matrix->row_pins[i], true, false);
    }

    for (uint8_t j = 0; j < KEY_MATRIX_COLS; ++j) {
        gpio_init(matrix->col_pins[j]);
        gpio_set_dir(matrix->col_pins[j], GPIO_OUT);
        gpio_put(matrix->col_pins[j], true);
    }

    key_matrix_clear(matrix);
    current_layer = _BASE;
}

bool key_matrix_scan(key_matrix_t *matrix) {
    bool changed = false;

    for (uint8_t j = 0; j < KEY_MATRIX_COLS; ++j) {
        gpio_put(matrix->col_pins[j], false);
        sleep_us(2);  // 信号安定待ち

        for (uint8_t i = 0; i < KEY_MATRIX_ROWS; ++i) {
            const bool raw_pressed = !gpio_get(matrix->row_pins[i]);

            if (raw_pressed) {
                // 押下優先：押下は即確定し、離鍵は防止期間の満了を待つ。
                if (!matrix->key_states[i][j]) {
                    matrix->key_states[i][j] = true;
                    changed = true;
                }
                matrix->debounce_ticks[i][j] = KEY_DEBOUNCE_TICKS;
            } else if (matrix->key_states[i][j]) {
                if (matrix->debounce_ticks[i][j] > 0) {
                    matrix->debounce_ticks[i][j]--;
                } else {
                    matrix->key_states[i][j] = false;
                    changed = true;
                }
            }
        }

        gpio_put(matrix->col_pins[j], true);
    }

    return changed;
}

static layer_t resolve_layer(const key_matrix_t *matrix) {
    bool fn_held = false;
    for (uint8_t i = 0; i < KEY_MATRIX_ROWS; ++i)
        for (uint8_t j = 0; j < KEY_MATRIX_COLS; ++j)
            if (matrix->key_states[i][j] && keymap[_BASE][i][j] == KC_FN)
                fn_held = true;
    if (!fn_held) return _BASE;
    for (uint8_t i = 0; i < KEY_MATRIX_ROWS; ++i)
        for (uint8_t j = 0; j < KEY_MATRIX_COLS; ++j)
            if (matrix->key_states[i][j] && keymap[_FN][i][j] == KC_FN2)
                return _FN2;
    return _FN;
}

void key_matrix_build_report(key_matrix_t *matrix,
                             keyboard_bitmap_report_t *report) {
    hid_report_clear(report);
    matrix->consumer_usage = 0;
    current_layer = resolve_layer(matrix);

    for (uint8_t i = 0; i < KEY_MATRIX_ROWS; ++i) {
        for (uint8_t j = 0; j < KEY_MATRIX_COLS; ++j) {
            if (!matrix->key_states[i][j]) {
                matrix->active_kc[i][j] = 0;
                continue;
            }

            // 押下した瞬間のレイヤーでキーコードを確定して離すまで保持する。これにより FN を先に離してもベースレイヤーのキーコードに化けない。
            uint16_t kc = matrix->active_kc[i][j];
            if (kc == 0) {
                // 透過キーはレイヤースタックに沿って _FN2 → _FN → _BASE の順でフォールスルーする。_FN2 から直接 _BASE へ飛ばすと、FN+Esc（_FN で KC_FN2）の位置が _FN2 では透過のため _BASE の KC_ESC に化けて Esc が入力されてしまう。
                kc = keymap[current_layer][i][j];
                if (kc == KC_TRANS && current_layer == _FN2)
                    kc = keymap[_FN][i][j];
                if (kc == KC_TRANS) kc = keymap[_BASE][i][j];
                matrix->active_kc[i][j] = kc;
                if (KC_IS_ACTION(kc) && matrix->pending_action == 0) {
                    matrix->pending_action = kc;
                }
            }

            if (KC_IS_MEDIA(kc)) {
                // Consumer Control のレポートは usage を 1 つしか運べないため、同時押しでは最初に見つけたキーを採用する。
                if (matrix->consumer_usage == 0) {
                    matrix->consumer_usage = KC_MEDIA_USAGE(kc);
                }
                continue;
            }
            if (kc == KC_FN || kc == KC_FN2 || kc == XXX || kc == KC_TRANS ||
                KC_IS_ACTION(kc))
                continue;
            const uint8_t kc8 = (uint8_t)kc;
            if (KC_IS_MODIFIER(kc8))
                report->modifier |= KC_MOD_BIT(kc8);
            else
                hid_report_set_key(report, kc8, true);
        }
    }
}

uint16_t key_matrix_consume_action(key_matrix_t *matrix) {
    const uint16_t action = matrix->pending_action;
    matrix->pending_action = 0;
    return action;
}

uint16_t key_matrix_get_consumer_usage(const key_matrix_t *matrix) {
    return matrix->consumer_usage;
}

void key_matrix_clear(key_matrix_t *matrix) {
    memset(matrix->key_states, 0, sizeof(matrix->key_states));
    memset(matrix->debounce_ticks, 0, sizeof(matrix->debounce_ticks));
    memset(matrix->active_kc, 0, sizeof(matrix->active_kc));
    matrix->pending_action = 0;
    matrix->consumer_usage = 0;
}

layer_t key_matrix_get_layer(void) { return current_layer; }
