#ifndef EHHHKB2_USB_HID_H
#define EHHHKB2_USB_HID_H

#include <stdbool.h>
#include <stdint.h>

#include "hid/hid_report.h"

void usb_hid_init(void);
void usb_hid_task(void);
void usb_hid_send_report_if_needed(const keyboard_bitmap_report_t *report);

// メディアキーの Consumer Control usage（0 = 離鍵）を送る。
void usb_hid_send_consumer_if_needed(uint16_t usage);

// ホストから通知された最新のキーボード LED 状態（HID_LED_* のビット和）。本機に物理 LED は無く、ディスプレイ表示にのみ使う。USB 切断時は 0 に戻る。BLE 接続時は取得できない（常に 0）。
uint8_t usb_hid_get_keyboard_leds(void);

bool usb_hid_is_connected(void);

#endif
