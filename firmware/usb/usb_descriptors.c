#include "usb/usb_descriptors.h"

#include <string.h>

#include "pico/unique_id.h"
#include "tusb.h"

#define USB_VID 0x2E8A
#define USB_PID 0x10A1
#define USB_BCD 0x0100

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = USB_BCD,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

// clang-format off
static uint8_t const hid_report_descriptor[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)

    // Consumer Control を別コレクションで並べるため Report ID が必要になる。以降のキーボードレポートは先頭 1 バイトに Report ID が付く。
    0x85, REPORT_ID_KEYBOARD,  // Report ID (1)

    0x75, 0x01,  // Report Size (1 bit)
    0x95, 0x08,  // Report Count (8 modifiers)
    0x05, 0x07,  // Usage Page (Keyboard/Keypad)
    0x19, 0xE0,  // Usage Minimum (Left Control)
    0x29, 0xE7,  // Usage Maximum (Right GUI)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x81, 0x02,  // Input (Data, Variable, Absolute): modifier bitmap

    0x75, 0x01,  // Report Size (1 bit)
    0x95, 0x05,  // Report Count (5 LEDs)
    0x05, 0x08,  // Usage Page (LEDs)
    0x19, 0x01,  // Usage Minimum (Num Lock)
    0x29, 0x05,  // Usage Maximum (Kana)
    0x91, 0x02,  // Output (Data, Variable, Absolute): LED states

    0x75, 0x03,  // Report Size (3 bits)
    0x95, 0x01,  // Report Count (1 field)
    0x91, 0x03,  // Output (Constant, Variable, Absolute): LED padding

    0x75, 0x01,  // Report Size (1 bit per key)
    0x95, 0x80,  // Report Count (128 keys)
    0x05, 0x07,  // Usage Page (Keyboard/Keypad)
    0x19, 0x00,  // Usage Minimum (Reserved / no event)
    0x29, 0x7F,  // Usage Maximum (Keyboard usage 0x7F)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x81, 0x02,  // Input (Data, Variable, Absolute): 128-bit key bitmap

    // usage 0x80 以上（JIS の ろ / ¥ など）は上の bitmap の範囲外なので、8bit キーコードの配列で送る。Logical/Usage Maximum は1バイト形式だと符号付きで-1と解釈されるため2バイト形式で 0x00FF を書く。
    0x75, 0x08,        // Report Size (8 bits)
    0x95, 0x02,        // Report Count (2 keys)
    0x05, 0x07,        // Usage Page (Keyboard/Keypad)
    0x15, 0x00,        // Logical Minimum (0)
    0x26, 0xFF, 0x00,  // Logical Maximum (255)
    0x19, 0x00,        // Usage Minimum (0)
    0x2A, 0xFF, 0x00,  // Usage Maximum (255)
    0x81, 0x00,        // Input (Data, Array, Absolute): usage 0x80 以上を 2 つ

    0xC0,  // End Collection

    // メディアキー。ホストがメディア操作として扱うには Consumer Control が独立したトップレベルコレクションである必要がある。押されている usage を16bitの配列項目で1つ送り、離鍵は0を送る。
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, REPORT_ID_CONSUMER,  // Report ID (2)
    0x75, 0x10,        // Report Size (16 bits)
    0x95, 0x01,        // Report Count (1 usage)
    0x15, 0x00,        // Logical Minimum (0)
    0x26, 0xFF, 0x03,  // Logical Maximum (0x03FF)
    0x19, 0x00,        // Usage Minimum (0)
    0x2A, 0xFF, 0x03,  // Usage Maximum (0x03FF)
    0x81, 0x00,        // Input (Data, Array, Absolute): usage を 1 つ
    0xC0,              // End Collection
};
// clang-format on

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
#define EPNUM_HID 0x81

// エンドポイントの bInterval。フルスピードではフレーム単位（= ms）なので 1ms ポーリング（1000Hz）になる。キースキャンも 1ms 周期なので揃えている。
#define HID_POLL_INTERVAL_MS 1

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_KEYBOARD, 0, HID_ITF_PROTOCOL_KEYBOARD,
                       sizeof(hid_report_descriptor), EPNUM_HID,
                       CFG_TUD_HID_EP_BUFSIZE, HID_POLL_INTERVAL_MS),
};

static char const *const string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "EhHHKB2",
    "Enhanced HHKB2",
    NULL,
};

static uint16_t desc_str[32 + 1];

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return hid_report_descriptor;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    switch (index) {
        case 0:
            memcpy(&desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;
            break;

        case 3: {
            pico_unique_board_id_t id;
            pico_get_unique_board_id(&id);
            static const char hex[] = "0123456789ABCDEF";
            chr_count = PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2;
            for (size_t i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; ++i) {
                desc_str[1 + i * 2] = hex[id.id[i] >> 4];
                desc_str[2 + i * 2] = hex[id.id[i] & 0x0F];
            }
            break;
        }

        default: {
            if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
                return NULL;
            }

            const char *str = string_desc_arr[index];
            chr_count = strlen(str);
            if (chr_count > 32) {
                chr_count = 32;
            }

            for (size_t i = 0; i < chr_count; ++i) {
                desc_str[1 + i] = str[i];
            }
            break;
        }
    }

    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | ((2 * chr_count) + 2));
    return desc_str;
}
