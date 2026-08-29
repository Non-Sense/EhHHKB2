#ifndef EHHHKB2_DISPLAY_POLICY_H
#define EHHHKB2_DISPLAY_POLICY_H

#include <stdbool.h>

#include "hid/key_matrix.h"
#include "pico/stdlib.h"

// ディスプレイ点灯判定。起動直後の一定時間は無条件で点灯（バッテリー駆動でも状態が見える）、コンフィグメニュー中と USB 接続中は常時点灯、それ以外は FN2 レイヤーへ入った瞬間から一定時間だけ点灯（wakeup）、ペアリングはコード表示中〜接続完了 +一定時間まで点灯。

typedef struct {
    bool config_active;
    bool usb_connected;
    bool has_code;             // パスキー表示中か
    bool connection_complete;  // BLE 接続完了の瞬間か（読み取りでクリア）
    layer_t layer;
} display_policy_input_t;

typedef struct {
    absolute_time_t startup_until;
    layer_t prev_layer;
    absolute_time_t fn2_until;
    bool fn2_active;
    bool pair_window;
    absolute_time_t pair_off_at;
    bool pair_off_armed;
} display_policy_t;

// 起動時に一度呼ぶ。initial_layer には key_matrix_get_layer() を渡す。
void display_policy_init(display_policy_t *policy, layer_t initial_layer);

// 毎ループ呼ぶ。点灯すべきなら true。
bool display_policy_update(display_policy_t *policy,
                           const display_policy_input_t *in);

#endif  // EHHHKB2_DISPLAY_POLICY_H
