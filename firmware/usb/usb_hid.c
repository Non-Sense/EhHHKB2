#include "usb/usb_hid.h"

#include <string.h>

#include "tusb.h"
#include "usb/usb_descriptors.h"

static keyboard_bitmap_report_t last_usb_report;
static uint16_t last_usb_consumer = 0;
static uint8_t usb_protocol_mode = HID_PROTOCOL_REPORT;

// ホストから SET_REPORT で通知される LED 状態（HID_LED_* のビット和）
static uint8_t keyboard_leds = 0;

void usb_hid_init(void) {
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO,
    };
    memset(&last_usb_report, 0, sizeof(last_usb_report));
    tusb_init(0, &dev_init);
}

void usb_hid_task(void) { tud_task(); }

bool usb_hid_is_connected(void) {
    // 自己給電デバイスでは VBUS 検出が USB コントローラに繋がっておらず、ケーブルを抜いても tud_mounted() が落ちない（VBUS 常時ありと扱われるため）。抜去するとホストの SOF が止まってバスがサスペンドするので、「マウント中 かつ 非サスペンド」で実際の接続を判定する。
    return tud_mounted() && !tud_suspended();
}

void usb_hid_send_report_if_needed(const keyboard_bitmap_report_t *report) {
    if (!tud_hid_ready()) {
        return;
    }

    if (hid_report_equal(report, &last_usb_report)) {
        return;
    }

    if (usb_protocol_mode == HID_PROTOCOL_BOOT) {
        // ブートプロトコルは Report ID を使わない固定 8 バイト。
        keyboard_boot_report_t boot_report;
        hid_report_to_boot(report, &boot_report);
        tud_hid_keyboard_report(0, boot_report.modifier, boot_report.keycodes);
    } else {
        tud_hid_report(REPORT_ID_KEYBOARD, report, sizeof(*report));
    }

    last_usb_report = *report;
}

void usb_hid_send_consumer_if_needed(const uint16_t usage) {
    if (!tud_hid_ready()) {
        return;
    }
    if (usage == last_usb_consumer) {
        return;
    }
    // ブートプロトコルには Consumer Control が無いので送れない。
    if (usb_protocol_mode == HID_PROTOCOL_BOOT) {
        return;
    }

    const uint8_t buf[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    if (!tud_hid_report(REPORT_ID_CONSUMER, buf, sizeof(buf))) {
        return;  // 次ループで再試行
    }
    last_usb_consumer = usage;
}

uint8_t usb_hid_get_keyboard_leds(void) { return keyboard_leds; }

void tud_mount_cb(void) {}

void tud_umount_cb(void) { keyboard_leds = 0; }

void tud_suspend_cb(bool remote_wakeup_en) { (void)remote_wakeup_en; }

void tud_resume_cb(void) {}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

// ホストからの SET_REPORT。キーボードの LED 状態（Num/Caps/Scroll/Compose/Kana）が 1 バイトで届く。物理 LED は無いのでディスプレイ表示にのみ使う。
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, const uint8_t *buffer,
                           uint16_t bufsize) {
    (void)report_id;

    if (instance != 0 || report_type != HID_REPORT_TYPE_OUTPUT || bufsize < 1) {
        return;
    }

    keyboard_leds = buffer[0];
}

void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
    (void)instance;
    usb_protocol_mode = protocol;
}

void tud_hid_report_complete_cb(uint8_t instance, const uint8_t *report,
                                uint16_t len) {
    (void)instance;
    (void)report;
    (void)len;
}
