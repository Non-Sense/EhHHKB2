#ifndef EHHHKB2_BLE_HID_H
#define EHHHKB2_BLE_HID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hid/hid_report.h"

#define BLE_HOST_SLOTS 6

// スロット名の最大文字数（終端を除く）
#define BLE_HOST_NAME_MAX 10

// ble_hid_get_active_slot() が未接続時に返す値
#define BLE_SLOT_NONE 0xFFu

void ble_hid_init(void);
void ble_hid_send_report_if_needed(const keyboard_bitmap_report_t* report);

// メディアキーの Consumer Control usage（0 = 離鍵）を送る。
void ble_hid_send_consumer_if_needed(uint16_t usage);

// バッテリー残量（0〜100）を Battery Service へ反映する。値が変わったときだけ通知し、通知間隔にも下限を設けるので毎ループ呼んでよい。
void ble_hid_update_battery_level(uint8_t percent);

bool ble_hid_is_connected(void);

// ホストが Output レポート（HIDS_SUBEVENT_SET_REPORT）で通知してきたキーボード LED 状態（HID_LED_* のビット和）。未接続時は 0。
uint8_t ble_hid_get_keyboard_leds(void);

// 接続が完了した瞬間か（読み取りでクリアされる）
bool ble_hid_is_connection_complete(void);

// ペアリングのコード（未取得なら 0）
uint32_t ble_hid_get_confirmation_code(void);
bool ble_hid_has_confirmation_code(void);

// BLE 接続先を切り替える。接続中ならまず切断し、対象スロットへ再アドバタイズする。
void ble_hid_switch_host(uint8_t slot);

uint8_t ble_hid_get_active_slot(void);

// ペアリングモードへ入り、オープンアドバタイズで新規デバイスを待つ
void ble_hid_start_pairing(void);

// ペアリングモードを離脱し、target_slot 向けの通常広告へ戻す。ペアリング中でなければ何もしない
void ble_hid_cancel_pairing(void);

// 全スロットのボンドと名前を削除し、広告を止めて待機する
void ble_hid_clear_all_bonds(void);

bool ble_hid_slot_has_bond(uint8_t slot);

// 指定スロットのボンドのみ削除する（接続中スロットなら切断する）
void ble_hid_reset_slot(uint8_t slot);

// スロット a と b の中身（ボンド・名前・Mac モード）を入れ替える。どちらかに接続中なら切断する（切断完了ハンドラで再アドバタイズされる）。
void ble_hid_swap_slots(uint8_t a, uint8_t b);

// 未設定なら空文字を返す
void ble_hid_get_slot_name(uint8_t slot, char* out, size_t n);

// フラッシュへ保存する。BLE_HOST_NAME_MAX 超は切り詰め、空文字で削除。
void ble_hid_set_slot_name(uint8_t slot, const char* name);

// スロットごとの Mac モード（Ctrl と GUI(Win) キーの usage を入れ替えて送信し、macOS 側で Ctrl キーを Command として扱わせる）。フラッシュへ永続化する。
bool ble_hid_get_mac_mode(uint8_t slot);
void ble_hid_set_mac_mode(uint8_t slot, bool enabled);

// BT を無効化する（接続中なら切断し、広告を停止して待機）。USB 接続のみで使う状態にする。BT キー(KC_BT1〜6)やペアリング開始で再び有効化される。
void ble_hid_disable_bt(void);

bool ble_hid_is_disabled(void);
bool ble_hid_is_pairing(void);

#endif
