#include <stdbool.h>
#include <string.h>

#include "ble/ble_hid.h"
#include "display/ssd1306.h"
#include "hid/hid_report.h"
#include "hid/key_matrix.h"
#include "hid/keycode.h"
#include "hid/output_router.h"
#include "pico/bootrom.h"
#include "pico/critical_section.h"
#include "pico/cyw43_arch.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "power/power_monitor.h"
#include "ui/config_menu.h"
#include "ui/display_policy.h"
#include "ui/screen.h"
#include "usb/usb_hid.h"

// 押下遅延はスキャン待ち（最悪これ 1 回分）と USB ポーリング 1ms の合計で決まる。これを縮めると key_matrix.c の KEY_DEBOUNCE_TICKS の実時間も比例して短くなるため、片方だけ変えてはならない。
#define KEY_SCAN_INTERVAL_US 500

static const uint8_t row_pins[KEY_MATRIX_ROWS] = {2, 3, 4, 5, 6, 7, 8, 9};
static const uint8_t col_pins[KEY_MATRIX_COLS] = {10, 11, 12, 13, 14, 15,
                                                  16, 17, 18, 19, 20};

// コア間共有状態: Core 0（キースキャン / BLE）→ Core 1（ディスプレイ）
static ui_state_t shared;
static critical_section_t shared_cs;

static key_matrix_t matrix;
static keyboard_bitmap_report_t current_report;

// ディスプレイ手動 OFF（コンフィグメニューの Display Off/On）。メニュー内で選び直すことでのみ解除される（メニューを開くだけでは戻らない。メニュー表示中は display_policy 側の config_active 判定で見えるようにする）。
static bool display_forced_off = false;

// Quiet mode（コンフィグメニューの Quiet On/Off）の現在値。内蔵 DC-DC の SMPS モードは WL_GPIO1 経由でのみ制御でき、High にすると効率が落ちる代わりにスイッチングノイズが低減する。
static bool quiet_mode_on = false;

static void core1_entry(void) {
    // Core0 のフラッシュ操作（BTstack のボンド保存）中に Core1 を安全に停止できるよう、ロックアウト対象として登録する。無いとボンド削除/保存でハングする。
    flash_safe_execute_core_init();

    while (true) {
        power_monitor_update();

        // 描画中に Core 0 の更新が混ざらないよう、まるごとスナップショットする。
        critical_section_enter_blocking(&shared_cs);
        const ui_state_t state = shared;
        critical_section_exit(&shared_cs);

        screen_task(&state);

        tight_loop_contents();
    }
}

// with_bond は le_device_db への問い合わせを伴うため、メニュー表示中だけ true。
static void read_host_state(cfg_host_state_t *host, const bool with_bond) {
    host->connected = ble_hid_is_connected();
    host->active_slot = ble_hid_get_active_slot();
    for (uint8_t i = 0; i < BLE_HOST_SLOTS; ++i) {
        ble_hid_get_slot_name(i, host->slot_name[i],
                              sizeof(host->slot_name[i]));
        host->slot_has_bond[i] = with_bond && ble_hid_slot_has_bond(i);
        host->slot_mac_mode[i] = ble_hid_get_mac_mode(i);
    }
}

static void run_config_action(const cfg_action_t *action) {
    switch (action->kind) {
        case CFG_ACTION_CONNECT_SLOT:
            ble_hid_switch_host(action->slot);
            break;
        case CFG_ACTION_DISABLE_BT:
            ble_hid_disable_bt();
            break;
        case CFG_ACTION_RESET_SLOT:
            ble_hid_reset_slot(action->slot);
            break;
        case CFG_ACTION_SET_SLOT_NAME:
            ble_hid_set_slot_name(action->slot, action->name);
            break;
        case CFG_ACTION_SET_MAC_MODE:
            ble_hid_set_mac_mode(action->slot, action->value);
            break;
        case CFG_ACTION_TOGGLE_PAIRING:
            if (ble_hid_is_pairing()) {
                ble_hid_cancel_pairing();
            } else {
                ble_hid_start_pairing();
            }
            break;
        case CFG_ACTION_SWAP_SLOTS:
            ble_hid_swap_slots(action->slot, action->slot_b);
            break;
        case CFG_ACTION_TOGGLE_DISPLAY:
            display_forced_off = !display_forced_off;
            break;
        case CFG_ACTION_TOGGLE_QUIET_MODE:
            quiet_mode_on = !quiet_mode_on;
            cyw43_arch_gpio_put(1, quiet_mode_on);
            break;
        case CFG_ACTION_REBOOT_BOOTLOADER:
            // Core 1 に "Booting..." を描画させてから再起動する。省電力でパネルが消えていても見えるよう display_on も強制的に立てる。
            critical_section_enter_blocking(&shared_cs);
            shared.booting = true;
            shared.display_on = true;
            critical_section_exit(&shared_cs);
            reset_usb_boot(0, 0);
            break;
        case CFG_ACTION_NONE:
        default:
            break;
    }
}

static void run_virtual_key(const uint16_t vkey, config_menu_t *menu,
                            keyboard_bitmap_report_t *report) {
    if (vkey >= KC_BT1 && vkey <= KC_BT6) {
        ble_hid_switch_host((uint8_t)(vkey - KC_BT1));
    } else if (vkey == KC_PAIR) {
        if (ble_hid_is_pairing()) {
            ble_hid_cancel_pairing();
        } else {
            ble_hid_start_pairing();
        }
    } else if (vkey == KC_BRST) {
        ble_hid_clear_all_bonds();
    } else if (vkey == KC_USB) {
        ble_hid_disable_bt();
    } else if (vkey == KC_CFG) {
        config_menu_open(menu, report);
        output_router_release_all();
    }
}

static void handle_scan_result(config_menu_t *menu,
                               const cfg_host_state_t *host,
                               const uint16_t vkey,
                               keyboard_bitmap_report_t *report) {
    if (config_menu_is_active(menu)) {
        const cfg_action_t action = config_menu_update(menu, host, report);
        run_config_action(&action);
    } else {
        run_virtual_key(vkey, menu, report);
    }
}

static void publish_ui_state(const config_menu_t *menu,
                             const cfg_host_state_t *host,
                             const output_router_t *router,
                             const bool usb_connected, const bool has_code,
                             const bool display_on) {
    critical_section_enter_blocking(&shared_cs);
    shared.host = *host;
    shared.pairing = ble_hid_is_pairing();
    shared.display_forced_off = display_forced_off;
    shared.quiet_mode_on = quiet_mode_on;
    shared.bt_disabled = ble_hid_is_disabled();
    shared.usb_connected = usb_connected;
    // キー入力を実際に送っている経路（USB ケーブル在中でも BLE へ切替済みのことがあるため、物理接続の usb_connected ではなく router を見る）。
    shared.leds = output_router_is_usb_active(router)
                      ? usb_hid_get_keyboard_leds()
                      : (ble_hid_is_connected() ? ble_hid_get_keyboard_leds()
                                                 : 0);
    shared.has_code = has_code;
    shared.code = ble_hid_get_confirmation_code();
    shared.display_on = display_on;
    shared.cfg_screen = menu->screen;
    shared.cfg_cursor = menu->cursor;
    shared.cfg_rename_slot = menu->rename_slot;
    shared.cfg_swap_from_slot = menu->swap_from_slot;
    memcpy(shared.cfg_edit, menu->edit_buf, sizeof(shared.cfg_edit));
    critical_section_exit(&shared_cs);
}

int main(void) {
    if (cyw43_arch_init() != 0) {
        return 1;
    }

    hid_report_clear(&current_report);

    key_matrix_init(&matrix, row_pins, col_pins);
    ssd1306_init();
    power_monitor_init();
    usb_hid_init();
    ble_hid_init();

    ssd1306_clear_buffer();
    ssd1306_display();

    critical_section_init(&shared_cs);
    shared.display_on = true;
    multicore_launch_core1(core1_entry);

    config_menu_t menu;
    config_menu_init(&menu);

    display_policy_t policy;
    display_policy_init(&policy, key_matrix_get_layer());

    output_router_t router;
    output_router_init(&router);

    absolute_time_t next_scan = make_timeout_time_us(KEY_SCAN_INTERVAL_US);

    // USB ホストとして enumerate した立ち上がりで BT を OFF にする。充電器のみでは usb_hid_is_connected() が立たないため誤発動しない。抜去時は自動復帰しない（BT1〜6 / ペアリング操作で手動復帰）。
    bool usb_was_connected = false;

    while (true) {
        usb_hid_task();
        cyw43_arch_poll();

        const bool usb_connected = usb_hid_is_connected();
        if (usb_connected && !usb_was_connected) {
            ble_hid_disable_bt();
        }
        usb_was_connected = usb_connected;

        // BTstack は Core 0 からしか触れないため、Core 1 が更新した電圧をここで読んで反映する。
        ble_hid_update_battery_level(power_monitor_get_battery_percent());

        // メニュー操作と表示の両方が同じ BLE 状態を見るよう、先に読み出す。
        const bool config_active = config_menu_is_active(&menu);
        cfg_host_state_t host;
        read_host_state(&host, config_active);

        if (time_reached(next_scan)) {
            if (key_matrix_scan(&matrix)) {
                key_matrix_build_report(&matrix, &current_report);
                const uint16_t vkey = key_matrix_consume_action(&matrix);
                handle_scan_result(&menu, &host, vkey, &current_report);
            }
            next_scan = make_timeout_time_us(KEY_SCAN_INTERVAL_US);
        }

        if (!config_menu_is_active(&menu)) {
            output_router_send(&router, &current_report,
                               key_matrix_get_consumer_usage(&matrix));
        }

        const bool has_code = ble_hid_has_confirmation_code();
        const display_policy_input_t policy_in = {
            .config_active = config_menu_is_active(&menu),
            .usb_connected = usb_connected,
            .has_code = has_code,
            .connection_complete = ble_hid_is_connection_complete(),
            .layer = key_matrix_get_layer(),
        };
        // display_forced_off 中も内部タイマーは進めておきたいので、display_policy_update() 自体は毎回呼んでから上書きする。ただしメニュー表示中は forced_off を無視して見えるようにする（でないと Display On を選び直す手段がなくなる）。
        const bool policy_display_on =
            display_policy_update(&policy, &policy_in);
        const bool display_on =
            policy_display_on &&
            (!display_forced_off || policy_in.config_active);

        publish_ui_state(&menu, &host, &router, usb_connected, has_code,
                         display_on);

        tight_loop_contents();
    }
}
