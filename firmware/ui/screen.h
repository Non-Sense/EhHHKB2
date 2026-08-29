#ifndef EHHHKB2_SCREEN_H
#define EHHHKB2_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "ble/ble_hid.h"
#include "ui/config_menu.h"

// Core 1 のディスプレイ表示。Core 0 が組み立てた ui_state_t のスナップショットだけを見て描画する。ここから BLE / USB / キーマトリクスの状態を直接読みに行ってはならない（コア間競合になる）。

typedef struct {
    cfg_host_state_t host;

    bool pairing;
    bool display_forced_off;  // Display Off メニューで手動消灯中か（表示ラベル用）
    bool quiet_mode_on;       // Quiet mode メニューで有効化しているか（表示ラベル用）
    bool bt_disabled;
    bool usb_connected;
    bool has_code;
    uint32_t code;
    bool display_on;  // ディスプレイを点灯すべきか（省電力制御）
    uint8_t leds;     // ホストが通知した LED 状態（HID_LED_* のビット和）

    // true の間は他の描画をすべて無視し、"Booting..." を中央表示する（ブートローダへ再起動する直前に立てる、一方通行のフラグ）。
    bool booting;

    cfg_screen_t cfg_screen;
    uint8_t cfg_cursor;
    uint8_t cfg_rename_slot;
    uint8_t cfg_swap_from_slot;
    char cfg_edit[BLE_HOST_NAME_MAX + 1];
} ui_state_t;

// Core 1 のループから毎周回呼ぶ。パネルの電源制御・再描画周期・変化検知による即時再描画は内部で行う。
void screen_task(const ui_state_t *state);

#endif  // EHHHKB2_SCREEN_H
