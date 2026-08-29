#ifndef EHHHKB2_KEY_MATRIX_H
#define EHHHKB2_KEY_MATRIX_H

#include <stdbool.h>
#include <stdint.h>

#include "hid/hid_report.h"

#define KEY_MATRIX_ROWS 8
#define KEY_MATRIX_COLS 11
#define KEY_MATRIX_TOTAL_KEYS (KEY_MATRIX_ROWS * KEY_MATRIX_COLS)

#define XXX 0x00  // 未使用キー

typedef enum { _BASE, _FN, _FN2, LAYER_NUM } layer_t;

// clang-format off
#define LAYOUT( \
          K12, K13, K14, K15,  K16, K17, K18, K19,  K1A, K1B, K78, K74, \
    K21, K22, K23, K24, K25, K26, K27, K28, K29, K2A, K2B, K7B, K77, K73, K71, K81, K82, \
     K31, K32, K33, K34, K35, K36, K37, K38, K39, K3A, K3B, K7A, K76,   K72,   K83, K84, \
      K41, K42, K43, K44, K45, K46, K47, K48, K49, K4A, K4B, K79,    K75,      K85, K88, \
       K51, K52, K53, K54, K55, K56, K57, K58, K59, K5A, K5B,       K65,  K67, K87, K89, \
       K61, K62, K63,             K64,                     K6A, K6B, K68, K66, K86, K8A  \
) { \
    { XXX, K12, K13, K14, K15, K16, K17, K18, K19, K1A, K1B }, \
    { K21, K22, K23, K24, K25, K26, K27, K28, K29, K2A, K2B }, \
    { K31, K32, K33, K34, K35, K36, K37, K38, K39, K3A, K3B }, \
    { K41, K42, K43, K44, K45, K46, K47, K48, K49, K4A, K4B }, \
    { K51, K52, K53, K54, K55, K56, K57, K58, K59, K5A, K5B }, \
    { K61, K62, K63, K64, K65, K66, K67, K68, XXX, K6A, K6B }, \
    { K71, K72, K73, K74, K75, K76, K77, K78, K79, K7A, K7B }, \
    { K81, K82, K83, K84, K85, K86, K87, K88, K89, K8A, XXX } \
}
// clang-format on

typedef struct {
    uint8_t row_pins[KEY_MATRIX_ROWS];
    uint8_t col_pins[KEY_MATRIX_COLS];
    bool key_states[KEY_MATRIX_ROWS][KEY_MATRIX_COLS];
    // 離鍵までのチャタリング防止残りカウント（押下検出で再武装）
    uint8_t debounce_ticks[KEY_MATRIX_ROWS][KEY_MATRIX_COLS];
    // 押下した瞬間に確定したキーコード（離すまで保持。0 = 未押下）
    uint16_t active_kc[KEY_MATRIX_ROWS][KEY_MATRIX_COLS];
    // 未消費の仮想キーアクション（0 = なし）。立ち上がりエッジで1回だけセットされる
    uint16_t pending_action;
    // 押下中のメディアキーの Consumer usage（0 = なし）。build_report で更新する
    uint16_t consumer_usage;
} key_matrix_t;

extern const uint16_t keymap[LAYER_NUM][KEY_MATRIX_ROWS][KEY_MATRIX_COLS];

void key_matrix_init(key_matrix_t* matrix, const uint8_t* row_pins,
                     const uint8_t* col_pins);

// デバウンス込みでスキャンし、状態が変化したら true を返す。
bool key_matrix_scan(key_matrix_t* matrix);

void key_matrix_build_report(key_matrix_t* matrix,
                             keyboard_bitmap_report_t* report);

// KC_BT1 等の仮想キーアクションを取得してクリアする（0 = なし）
uint16_t key_matrix_consume_action(key_matrix_t* matrix);

// 押下中のメディアキーの Consumer Control usage（0 = なし）。レポートが 1 usage しか運べないため、同時押しでは 1 つだけ返る。
uint16_t key_matrix_get_consumer_usage(const key_matrix_t* matrix);

void key_matrix_clear(key_matrix_t* matrix);
layer_t key_matrix_get_layer(void);

#endif  // EHHHKB2_KEY_MATRIX_H