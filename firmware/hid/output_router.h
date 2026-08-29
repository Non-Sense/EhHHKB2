#ifndef EHHHKB2_OUTPUT_ROUTER_H
#define EHHHKB2_OUTPUT_ROUTER_H

#include <stdbool.h>
#include <stdint.h>

#include "hid/hid_report.h"

// USB と BLE の両方がつながっていても同時には送らず、「最後に接続が確立した方」だけへ送る。切り替わったときは旧アクティブ側へ空レポートを送り、キーの押しっぱなしが残らないようにする。

typedef struct {
    bool active_is_usb;  // false=BLE, true=USB
    bool prev_usb_on;
    bool prev_ble_on;
} output_router_t;

void output_router_init(output_router_t *router);

// 現在キー入力を実際に送っている経路が USB か（false なら BLE）。usb_hid_is_connected() 等の物理接続状態とは異なり、切替の遅延判定（立ち上がりエッジ / フォールバック）を経た「実際にアクティブな経路」を返す。
bool output_router_is_usb_active(const output_router_t *router);

// consumer はメディアキーの Consumer Control usage（0 = 離鍵）。
void output_router_send(output_router_t *router,
                        const keyboard_bitmap_report_t *report,
                        uint16_t consumer);

// USB / BLE の両方へ空レポートを送り、押下中のキーを解放する。コンフィグメニューへ入るときなど、以降 HID 送信を止める直前に呼ぶ。
void output_router_release_all(void);

#endif  // EHHHKB2_OUTPUT_ROUTER_H
