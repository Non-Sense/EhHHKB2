#include "ui/display_policy.h"

// 起動直後この時間はバッテリー駆動でもディスプレイを点灯する
#define STARTUP_DISPLAY_MS 5000
// FN2 レイヤーに入った瞬間から点灯する時間（wakeup）
#define FN2_DISPLAY_MS 10000
// ペアリング完了後に点灯を維持する時間
#define PAIR_DISPLAY_MS 5000

void display_policy_init(display_policy_t *policy,
                         const layer_t initial_layer) {
    policy->startup_until = make_timeout_time_ms(STARTUP_DISPLAY_MS);
    policy->prev_layer = initial_layer;
    policy->fn2_until = (absolute_time_t){0};
    policy->fn2_active = false;
    policy->pair_window = false;
    policy->pair_off_at = (absolute_time_t){0};
    policy->pair_off_armed = false;
}

bool display_policy_update(display_policy_t *policy,
                           const display_policy_input_t *in) {
    // 接続状態に関わらず効かせる（バッテリー駆動・未接続でも画面を起こせる）。
    if (in->layer == _FN2 && policy->prev_layer != _FN2) {
        policy->fn2_until = make_timeout_time_ms(FN2_DISPLAY_MS);
        policy->fn2_active = true;
    }
    policy->prev_layer = in->layer;
    if (policy->fn2_active && time_reached(policy->fn2_until)) {
        policy->fn2_active = false;
    }

    // ペアリング表示ウィンドウ：コード表示中に開始し、接続完了 +PAIR_DISPLAY_MS で終了。
    if (in->has_code) {
        policy->pair_window = true;
        policy->pair_off_armed = false;
    }
    if (in->connection_complete && policy->pair_window) {
        policy->pair_off_at = make_timeout_time_ms(PAIR_DISPLAY_MS);
        policy->pair_off_armed = true;
    }
    if (policy->pair_window && policy->pair_off_armed &&
        time_reached(policy->pair_off_at)) {
        policy->pair_window = false;
        policy->pair_off_armed = false;
    }

    return in->config_active || !time_reached(policy->startup_until) ||
           in->usb_connected || policy->fn2_active || policy->pair_window;
}
