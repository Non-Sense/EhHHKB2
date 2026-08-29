#include "hid/hid_report.h"

#include <string.h>

void hid_report_clear(keyboard_bitmap_report_t *report) {
    memset(report, 0, sizeof(*report));
}

// usage 0x80 以上を配列スロットへ出し入れする。同じキーが既にあれば再利用し、空きが無ければ捨てる（同時押し上限）。
static void hid_report_set_extra_key(keyboard_bitmap_report_t *report,
                                     const uint8_t keycode,
                                     const bool pressed) {
    for (uint8_t i = 0; i < KEY_EXTRA_KEYS; ++i) {
        if (report->extra[i] == keycode) {
            if (!pressed) {
                report->extra[i] = 0;
            }
            return;
        }
    }
    if (!pressed) {
        return;
    }
    for (uint8_t i = 0; i < KEY_EXTRA_KEYS; ++i) {
        if (report->extra[i] == 0) {
            report->extra[i] = keycode;
            return;
        }
    }
}

void hid_report_set_key(keyboard_bitmap_report_t *report, uint8_t keycode,
                        bool pressed) {
    if (keycode >= KEY_BITMAP_BITS) {
        hid_report_set_extra_key(report, keycode, pressed);
        return;
    }

    uint8_t *byte = &report->bitmap[keycode / 8];
    const uint8_t mask = (uint8_t)(1u << (keycode & 7u));
    if (pressed) {
        *byte |= mask;
    } else {
        *byte &= (uint8_t)~mask;
    }
}

bool hid_report_get_key(const keyboard_bitmap_report_t *report,
                        const uint8_t keycode) {
    if (keycode >= KEY_BITMAP_BITS) {
        for (uint8_t i = 0; i < KEY_EXTRA_KEYS; ++i) {
            if (report->extra[i] == keycode) {
                return true;
            }
        }
        return false;
    }
    const uint8_t mask = (uint8_t)(1u << (keycode & 7u));
    return (report->bitmap[keycode / 8] & mask) != 0;
}

bool hid_report_equal(const keyboard_bitmap_report_t *lhs,
                      const keyboard_bitmap_report_t *rhs) {
    return memcmp(lhs, rhs, sizeof(*lhs)) == 0;
}

void hid_report_to_boot(const keyboard_bitmap_report_t *bitmap_report,
                        keyboard_boot_report_t *boot_report) {
    memset(boot_report, 0, sizeof(*boot_report));
    boot_report->modifier = bitmap_report->modifier;

    uint8_t out_index = 0;
    for (uint8_t keycode = 0; keycode < KEY_BITMAP_BITS && out_index < 6;
         ++keycode) {
        if (hid_report_get_key(bitmap_report, keycode)) {
            boot_report->keycodes[out_index++] = keycode;
        }
    }
    // ブートレポートのキーコードは 8bit なので usage 0x80 以上もそのまま入る。
    for (uint8_t i = 0; i < KEY_EXTRA_KEYS && out_index < 6; ++i) {
        if (bitmap_report->extra[i] != 0) {
            boot_report->keycodes[out_index++] = bitmap_report->extra[i];
        }
    }
}
