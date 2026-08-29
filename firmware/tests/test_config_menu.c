// ui/config_menu.c のホスト単体テスト。ハードウェア非依存の状態機械なので、キーレポートを合成して遷移を検証できる。

#include <stdio.h>
#include <string.h>

#include "hid/keycode.h"
#include "ui/config_menu.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        ++checks;                                                  \
        if (!(cond)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                            \
        }                                                          \
    } while (0)

// ---- テスト用ヘルパ ----

static keyboard_bitmap_report_t report;

static void press(const uint8_t kc) { hid_report_set_key(&report, kc, true); }
static void release(const uint8_t kc) {
    hid_report_set_key(&report, kc, false);
}

static cfg_action_t step(config_menu_t *menu, const cfg_host_state_t *host) {
    return config_menu_update(menu, host, &report);
}

static cfg_action_t tap(config_menu_t *menu, const cfg_host_state_t *host,
                        const uint8_t kc) {
    press(kc);
    const cfg_action_t action = step(menu, host);
    release(kc);
    step(menu, host);
    return action;
}

// 表示 BT1/BT2（一覧の上から1・2番目）がボンド済み、BT2 に接続中という標準的な状態。内部のスロット番号は表示と逆順（BT1=スロット3, BT2=スロット2, ...）なので、cfg_slot_for_row() を通した値を使う。
static cfg_host_state_t make_host(void) {
    cfg_host_state_t h;
    memset(&h, 0, sizeof(h));
    h.connected = true;
    h.active_slot = cfg_slot_for_row(1);  // 表示 BT2
    h.slot_has_bond[cfg_slot_for_row(0)] = true;  // 表示 BT1
    h.slot_has_bond[cfg_slot_for_row(1)] = true;  // 表示 BT2
    snprintf(h.slot_name[cfg_slot_for_row(0)], sizeof(h.slot_name[0]), "pc");
    return h;
}

static void open_menu(config_menu_t *menu) {
    config_menu_init(menu);
    hid_report_clear(&report);
    config_menu_open(menu, &report);
}

// ---- テストケース ----

static void test_init_and_open(void) {
    config_menu_t menu;
    config_menu_init(&menu);
    CHECK(menu.screen == CFG_OFF);
    CHECK(!config_menu_is_active(&menu));

    hid_report_clear(&report);
    press(KC_ENTER);
    const cfg_host_state_t host = make_host();
    CHECK(step(&menu, &host).kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_OFF);

    config_menu_open(&menu, &report);
    CHECK(menu.screen == CFG_TOP);
    CHECK(menu.cursor == 0);
    CHECK(config_menu_is_active(&menu));
    CHECK(!hid_report_get_key(&report, KC_ENTER));
}

static void test_item_counts(void) {
    CHECK(cfg_item_count(CFG_OFF) == 0);
    CHECK(cfg_item_count(CFG_TOP) == CFG_TOP_COUNT);
    CHECK(cfg_item_count(CFG_CONNECT) == BLE_HOST_SLOTS + 1);
    CHECK(cfg_item_count(CFG_RESET) == BLE_HOST_SLOTS);
    CHECK(cfg_item_count(CFG_RENAME) == BLE_HOST_SLOTS);
    CHECK(cfg_item_count(CFG_MACMODE) == BLE_HOST_SLOTS);
    CHECK(cfg_item_count(CFG_SWAP) == BLE_HOST_SLOTS);
    CHECK(cfg_item_count(CFG_SWAP_TARGET) == BLE_HOST_SLOTS);
    CHECK(cfg_item_count(CFG_MISC) == CFG_MISC_COUNT);
    CHECK(cfg_item_count(CFG_EDIT) == 0);
    CHECK(cfg_item_count(CFG_BATTERY) == 1);
    CHECK(cfg_item_count(CFG_BOOT) == 1);

    CHECK(CFG_ITEM_MAX >= CFG_TOP_COUNT);
    CHECK(CFG_ITEM_MAX >= BLE_HOST_SLOTS + 1);
    CHECK(CFG_ITEM_MAX >= CFG_MISC_COUNT);
}

static void test_cursor_wrap_and_edge(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);

    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == 1);

    // 押しっぱなしではリピートしない（立ち上がりのみ）
    press(KC_DOWN);
    step(&menu, &host);
    step(&menu, &host);
    step(&menu, &host);
    CHECK(menu.cursor == 2);
    release(KC_DOWN);
    step(&menu, &host);

    tap(&menu, &host, KC_UP);
    tap(&menu, &host, KC_UP);
    CHECK(menu.cursor == 0);
    tap(&menu, &host, KC_UP);
    CHECK(menu.cursor == CFG_TOP_COUNT - 1);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == 0);
}

static void test_escape_navigation(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);

    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_CONNECT);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_ESC);
    CHECK(menu.screen == CFG_TOP);
    CHECK(menu.cursor == 0);

    press(KC_ESC);
    step(&menu, &host);
    CHECK(menu.screen == CFG_OFF);
    CHECK(!config_menu_is_active(&menu));
    CHECK(!hid_report_get_key(&report, KC_ESC));
}

static void test_connect_screen(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_CONNECT);

    CHECK(menu.cursor == 0);
    cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_DISABLE_BT);
    CHECK(menu.screen == CFG_OFF);

    open_menu(&menu);
    tap(&menu, &host, KC_ENTER);
    tap(&menu, &host, KC_DOWN);  // cursor=1 (= 表示 BT1)
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_CONNECT_SLOT);
    CHECK(a.slot == cfg_slot_for_row(0));
    CHECK(menu.screen == CFG_OFF);

    open_menu(&menu);
    tap(&menu, &host, KC_ENTER);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);  // cursor=2 (= active_slot の表示行)
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_CONNECT);

    tap(&menu, &host, KC_DOWN);  // cursor=3
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_CONNECT);
}

static void test_reset_screen(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);  // TOP/BLE reset
    CHECK(menu.cursor == CFG_TOP_RESET);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_RESET);

    cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_RESET_SLOT);
    CHECK(a.slot == cfg_slot_for_row(0));
    CHECK(menu.screen == CFG_RESET);

    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);  // cursor=2
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_NONE);
}

static void test_rename_and_edit(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);  // TOP/BLE rename
    CHECK(menu.cursor == CFG_TOP_RENAME);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_RENAME);

    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_EDIT);
    CHECK(menu.rename_slot == cfg_slot_for_row(0));
    CHECK(strcmp(menu.edit_buf, "pc") == 0);
    CHECK(menu.edit_len == 2);

    tap(&menu, &host, KC_1);
    CHECK(strcmp(menu.edit_buf, "pc1") == 0);
    report.modifier = KC_MOD_BIT(KC_LSFT);
    tap(&menu, &host, KC_X);
    report.modifier = 0;
    CHECK(strcmp(menu.edit_buf, "pc1X") == 0);
    tap(&menu, &host, KC_MIN);
    CHECK(strcmp(menu.edit_buf, "pc1X-") == 0);

    tap(&menu, &host, KC_BSPC);
    CHECK(strcmp(menu.edit_buf, "pc1X") == 0);

    const cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_SET_SLOT_NAME);
    CHECK(a.slot == cfg_slot_for_row(0));
    CHECK(a.name != NULL && strcmp(a.name, "pc1X") == 0);
    CHECK(menu.screen == CFG_RENAME);
    CHECK(menu.cursor == 0);
}

static void test_edit_cancel_and_length_clamp(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_ENTER);  // → CFG_RENAME
    tap(&menu, &host, KC_DOWN);   // 表示 BT2（名前なし）
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_EDIT);
    CHECK(menu.edit_buf[0] == 0);

    for (int i = 0; i < BLE_HOST_NAME_MAX + 5; ++i) {
        tap(&menu, &host, KC_A);
    }
    CHECK(menu.edit_len == BLE_HOST_NAME_MAX);
    CHECK(strlen(menu.edit_buf) == BLE_HOST_NAME_MAX);

    const cfg_action_t a = tap(&menu, &host, KC_ESC);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_RENAME);
    CHECK(menu.cursor == 1);
}

static void test_macmode_screen(void) {
    cfg_host_state_t host = make_host();
    host.slot_mac_mode[cfg_slot_for_row(0)] = true;  // 表示 BT1 は ON 済み
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == CFG_TOP_MACMODE);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_MACMODE);

    cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_SET_MAC_MODE);
    CHECK(a.slot == cfg_slot_for_row(0));
    CHECK(a.value == false);
    CHECK(menu.screen == CFG_MACMODE);

    tap(&menu, &host, KC_DOWN);
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_SET_MAC_MODE);
    CHECK(a.slot == cfg_slot_for_row(1));
    CHECK(a.value == true);
}

static void test_swap_screen(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == CFG_TOP_SWAP);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_SWAP);

    CHECK(menu.cursor == 0);
    cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_SWAP_TARGET);
    CHECK(menu.swap_from_slot == cfg_slot_for_row(0));

    CHECK(menu.cursor == 0);
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_SWAP_TARGET);

    tap(&menu, &host, KC_DOWN);
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_SWAP_SLOTS);
    CHECK(a.slot == cfg_slot_for_row(0));
    CHECK(a.slot_b == cfg_slot_for_row(1));
    CHECK(menu.screen == CFG_SWAP);
    CHECK(menu.cursor == 0);

    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_SWAP_TARGET);
    CHECK(menu.swap_from_slot == cfg_slot_for_row(0));
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_SWAP_SLOTS);
    CHECK(a.slot == cfg_slot_for_row(0));
    CHECK(a.slot_b == cfg_slot_for_row(2));
    CHECK(menu.screen == CFG_SWAP);
}

static void test_battery_screen(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);

    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == CFG_TOP_MISC);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_MISC);
    CHECK(menu.cursor == CFG_MISC_BATTERY);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_BATTERY);

    cfg_action_t a = tap(&menu, &host, KC_DOWN);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.cursor == 0);
    a = tap(&menu, &host, KC_UP);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.cursor == 0);
    a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_BATTERY);

    // Esc は Misc へ、もう一度 Esc で Top へ戻る（階層ナビゲーション）。
    tap(&menu, &host, KC_ESC);
    CHECK(menu.screen == CFG_MISC);
    tap(&menu, &host, KC_ESC);
    CHECK(menu.screen == CFG_TOP);
}

static void test_display_toggle_menu_item(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == CFG_TOP_MISC);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_MISC);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == CFG_MISC_DISPLAY_OFF);
    const cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_TOGGLE_DISPLAY);
    CHECK(menu.screen == CFG_OFF);
}

static void test_quiet_mode_toggle_menu_item(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == CFG_TOP_MISC);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_MISC);
    tap(&menu, &host, KC_DOWN);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == CFG_MISC_QUIET_MODE);
    const cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_TOGGLE_QUIET_MODE);
    CHECK(menu.screen == CFG_OFF);
}

static void test_pairing_menu_item(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_DOWN);
    CHECK(menu.cursor == CFG_TOP_PAIR);
    const cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_TOGGLE_PAIRING);
    CHECK(menu.screen == CFG_OFF);
}

static void test_boot_screen(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);
    tap(&menu, &host, KC_UP);
    CHECK(menu.cursor == CFG_TOP_MISC);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_MISC);
    tap(&menu, &host, KC_UP);
    CHECK(menu.cursor == CFG_MISC_BOOT);
    tap(&menu, &host, KC_ENTER);
    CHECK(menu.screen == CFG_BOOT);

    const cfg_action_t a = tap(&menu, &host, KC_ENTER);
    CHECK(a.kind == CFG_ACTION_REBOOT_BOOTLOADER);
}

// 回帰テスト: トップ画面で Esc と Enter が同一スキャンで立ち上がると、旧実装では CFG_OFF が CFG_BOOT 扱いにフォールスルーして意図せずブートローダへ再起動していた。
static void test_esc_and_enter_same_scan(void) {
    const cfg_host_state_t host = make_host();
    config_menu_t menu;
    open_menu(&menu);

    press(KC_ESC);
    press(KC_ENTER);
    const cfg_action_t a = step(&menu, &host);
    CHECK(a.kind == CFG_ACTION_NONE);
    CHECK(menu.screen == CFG_OFF);
}

int main(void) {
    test_init_and_open();
    test_item_counts();
    test_cursor_wrap_and_edge();
    test_escape_navigation();
    test_connect_screen();
    test_reset_screen();
    test_rename_and_edit();
    test_edit_cancel_and_length_clamp();
    test_macmode_screen();
    test_swap_screen();
    test_battery_screen();
    test_display_toggle_menu_item();
    test_quiet_mode_toggle_menu_item();
    test_pairing_menu_item();
    test_boot_screen();
    test_esc_and_enter_same_scan();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
