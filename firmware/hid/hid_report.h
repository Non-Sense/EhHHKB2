#ifndef EHHHKB2_HID_REPORT_H
#define EHHHKB2_HID_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#define KEY_BITMAP_BITS 128
#define KEY_BITMAP_BYTES (KEY_BITMAP_BITS / 8)

// bitmap は usage 0x00-0x7F しか表現できない。JIS の ろ / ¥ / 変換 / 無変換やかな / 英数（0x87 以降）はその範囲外なので、8bit キーコードの配列で送る。同時押しはこの数まで。IME 系キーを重ねて押すことはないため 2 で足りる。
#define KEY_EXTRA_KEYS 2

// ホストが通知するキーボード LED のビット（HID Usage Page 0x08 の並び順）。レポートディスクリプタの Output 項目と 1 対 1 で対応する。
#define HID_LED_NUM_LOCK 0x01u
#define HID_LED_CAPS_LOCK 0x02u
#define HID_LED_SCROLL_LOCK 0x04u
#define HID_LED_COMPOSE 0x08u
#define HID_LED_KANA 0x10u

typedef struct {
    uint8_t modifier;
    uint8_t bitmap[KEY_BITMAP_BYTES];
    uint8_t extra[KEY_EXTRA_KEYS];  // usage 0x80 以上（0 = 空きスロット）
} keyboard_bitmap_report_t;

typedef struct {
    uint8_t modifier;
    uint8_t reserved;
    uint8_t keycodes[6];
} keyboard_boot_report_t;

void hid_report_clear(keyboard_bitmap_report_t *report);
void hid_report_set_key(keyboard_bitmap_report_t *report, uint8_t keycode,
                        bool pressed);
bool hid_report_get_key(const keyboard_bitmap_report_t *report,
                        uint8_t keycode);
bool hid_report_equal(const keyboard_bitmap_report_t *lhs,
                      const keyboard_bitmap_report_t *rhs);
void hid_report_to_boot(const keyboard_bitmap_report_t *bitmap_report,
                        keyboard_boot_report_t *boot_report);

#endif
