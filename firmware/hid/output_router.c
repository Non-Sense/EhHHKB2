#include "hid/output_router.h"

#include "ble/ble_hid.h"
#include "usb/usb_hid.h"

void output_router_init(output_router_t *router) {
    router->active_is_usb = false;
    router->prev_usb_on = false;
    router->prev_ble_on = false;
}

void output_router_send(output_router_t *router,
                        const keyboard_bitmap_report_t *report,
                        const uint16_t consumer) {
    const bool usb_on = usb_hid_is_connected();
    const bool ble_on = ble_hid_is_connected();
    const bool was_usb = router->active_is_usb;

    // 接続確立の立ち上がりだけで切り替える（両方つながっていても動かない）。
    if (usb_on && !router->prev_usb_on) {
        router->active_is_usb = true;
    } else if (ble_on && !router->prev_ble_on) {
        router->active_is_usb = false;
    }
    // アクティブ側が切れて他方が生きていればフォールバックする。
    if (router->active_is_usb && !usb_on && ble_on) {
        router->active_is_usb = false;
    } else if (!router->active_is_usb && !ble_on && usb_on) {
        router->active_is_usb = true;
    }
    router->prev_usb_on = usb_on;
    router->prev_ble_on = ble_on;

    // 切替元に空レポートを送り、キー押しっぱなしの残留を解放する。
    if (router->active_is_usb != was_usb) {
        keyboard_bitmap_report_t rel;
        hid_report_clear(&rel);
        if (router->active_is_usb) {
            ble_hid_send_report_if_needed(&rel);
            ble_hid_send_consumer_if_needed(0);
        } else {
            usb_hid_send_report_if_needed(&rel);
            usb_hid_send_consumer_if_needed(0);
        }
    }

    if (router->active_is_usb) {
        usb_hid_send_report_if_needed(report);
        usb_hid_send_consumer_if_needed(consumer);
    } else {
        ble_hid_send_report_if_needed(report);
        ble_hid_send_consumer_if_needed(consumer);
    }
}

bool output_router_is_usb_active(const output_router_t *router) {
    return router->active_is_usb;
}

void output_router_release_all(void) {
    keyboard_bitmap_report_t rel;
    hid_report_clear(&rel);
    usb_hid_send_report_if_needed(&rel);
    usb_hid_send_consumer_if_needed(0);
    ble_hid_send_report_if_needed(&rel);
    ble_hid_send_consumer_if_needed(0);
}
