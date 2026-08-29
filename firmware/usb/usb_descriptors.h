#ifndef EHHHKB2_USB_DESCRIPTORS_H
#define EHHHKB2_USB_DESCRIPTORS_H

enum {
    ITF_NUM_KEYBOARD = 0,
    ITF_NUM_TOTAL,
};

// レポートディスクリプタが宣言する Report ID。BLE 側（ble_hid.c）も同じ番号を使う。
enum {
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_CONSUMER = 2,
};

#endif
