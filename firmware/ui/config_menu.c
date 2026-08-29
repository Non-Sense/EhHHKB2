#include "ui/config_menu.h"

#include <string.h>

#include "hid/keycode.h"

// 名前入力に使える文字（英数字・空白・ハイフン）へ変換する。対象外は 0。
static char keycode_to_char(const uint8_t kc, const bool shift) {
    if (kc >= KC_A && kc <= KC_Z) {
        const char c = (char)('a' + (kc - KC_A));
        return shift ? (char)(c - 32) : c;
    }
    if (kc >= KC_1 && kc <= KC_9) return (char)('1' + (kc - KC_1));
    if (kc == KC_0) return '0';
    if (kc == KC_SPACE) return ' ';
    if (kc == KC_MIN) return shift ? '_' : '-';
    return 0;
}

uint8_t cfg_item_count(const cfg_screen_t screen) {
    switch (screen) {
        case CFG_TOP:
            return CFG_TOP_COUNT;
        case CFG_CONNECT:
            return BLE_HOST_SLOTS + 1;
        case CFG_RESET:
        case CFG_RENAME:
        case CFG_MACMODE:
        case CFG_SWAP:
        case CFG_SWAP_TARGET:
            return BLE_HOST_SLOTS;
        case CFG_MISC:
            return CFG_MISC_COUNT;
        case CFG_BATTERY:
        case CFG_BOOT:
            return 1;
        default:
            return 0;
    }
}

// Esc で戻る先の画面。CFG_BATTERY / CFG_BOOT は CFG_MISC の子画面なので Misc へ、他は CFG_TOP へ戻る。
static cfg_screen_t cfg_parent_screen(const cfg_screen_t screen) {
    switch (screen) {
        case CFG_BATTERY:
        case CFG_BOOT:
            return CFG_MISC;
        default:
            return CFG_TOP;
    }
}

void config_menu_init(config_menu_t *menu) {
    memset(menu, 0, sizeof(*menu));
    menu->screen = CFG_OFF;
    hid_report_clear(&menu->edit_prev);
}

bool config_menu_is_active(const config_menu_t *menu) {
    return menu->screen != CFG_OFF;
}

void config_menu_open(config_menu_t *menu, keyboard_bitmap_report_t *report) {
    menu->screen = CFG_TOP;
    menu->cursor = 0;
    menu->nav_prev_up = menu->nav_prev_down = false;
    menu->nav_prev_enter = menu->nav_prev_esc = false;
    hid_report_clear(report);
}

static cfg_action_t config_menu_update_edit(config_menu_t *menu,
                                            const keyboard_bitmap_report_t *r) {
    cfg_action_t action = {.kind = CFG_ACTION_NONE};
    const uint8_t shift_mask = KC_MOD_BIT(KC_LSFT) | KC_MOD_BIT(KC_RSFT);
    const bool shift = (r->modifier & shift_mask) != 0;

    for (uint8_t kc = KC_A; kc < KEY_BITMAP_BITS; ++kc) {
        const bool now = hid_report_get_key(r, kc);
        const bool was = hid_report_get_key(&menu->edit_prev, kc);
        if (!now || was) continue;  // 立ち上がりのみ
        if (kc == KC_ENTER) {
            action.kind = CFG_ACTION_SET_SLOT_NAME;
            action.slot = menu->rename_slot;
            action.name = menu->edit_buf;
            menu->screen = CFG_RENAME;
            // cfg_slot_for_row() は自己逆変換（行→スロットと同じ式でスロット→行にも戻せる）なので、そのまま流用する。
            menu->cursor = cfg_slot_for_row(menu->rename_slot);
        } else if (kc == KC_ESC) {
            menu->screen = CFG_RENAME;
            menu->cursor = cfg_slot_for_row(menu->rename_slot);
        } else if (kc == KC_BSPC) {
            if (menu->edit_len > 0) menu->edit_buf[--menu->edit_len] = 0;
        } else {
            const char c = keycode_to_char(kc, shift);
            if (c && menu->edit_len < BLE_HOST_NAME_MAX) {
                menu->edit_buf[menu->edit_len++] = c;
                menu->edit_buf[menu->edit_len] = 0;
            }
        }
    }
    menu->edit_prev = *r;
    return action;
}

static cfg_action_t config_menu_confirm(config_menu_t *menu,
                                        const cfg_host_state_t *host,
                                        keyboard_bitmap_report_t *report) {
    cfg_action_t action = {.kind = CFG_ACTION_NONE};

    if (menu->screen == CFG_TOP) {
        switch (menu->cursor) {
            case CFG_TOP_CONNECT:
                menu->screen = CFG_CONNECT;
                break;
            case CFG_TOP_PAIR:
                action.kind = CFG_ACTION_TOGGLE_PAIRING;
                menu->screen = CFG_OFF;
                hid_report_clear(report);
                break;
            case CFG_TOP_MACMODE:
                menu->screen = CFG_MACMODE;
                break;
            case CFG_TOP_RENAME:
                menu->screen = CFG_RENAME;
                break;
            case CFG_TOP_SWAP:
                menu->screen = CFG_SWAP;
                break;
            case CFG_TOP_RESET:
                menu->screen = CFG_RESET;
                break;
            case CFG_TOP_MISC:
                menu->screen = CFG_MISC;
                break;
            default:
                break;
        }
        menu->cursor = 0;
    } else if (menu->screen == CFG_MISC) {
        switch (menu->cursor) {
            case CFG_MISC_BATTERY:
                menu->screen = CFG_BATTERY;
                break;
            case CFG_MISC_DISPLAY_OFF:
                action.kind = CFG_ACTION_TOGGLE_DISPLAY;
                menu->screen = CFG_OFF;
                hid_report_clear(report);
                break;
            case CFG_MISC_QUIET_MODE:
                action.kind = CFG_ACTION_TOGGLE_QUIET_MODE;
                menu->screen = CFG_OFF;
                hid_report_clear(report);
                break;
            default:  // CFG_MISC_BOOT → 確認画面
                menu->screen = CFG_BOOT;
                break;
        }
        menu->cursor = 0;
    } else if (menu->screen == CFG_RENAME) {
        menu->rename_slot = cfg_slot_for_row(menu->cursor);
        const char *cur = host->slot_name[menu->rename_slot];
        size_t i = 0;
        for (; i < BLE_HOST_NAME_MAX && cur[i]; ++i) menu->edit_buf[i] = cur[i];
        menu->edit_buf[i] = 0;
        menu->edit_len = (uint8_t)i;
        // 押下中の Enter を入力扱いしないよう前回状態を現在に
        menu->edit_prev = *report;
        menu->screen = CFG_EDIT;
    } else if (menu->screen == CFG_CONNECT) {
        if (menu->cursor == 0) {
            action.kind = CFG_ACTION_DISABLE_BT;
            menu->screen = CFG_OFF;
            hid_report_clear(report);
        } else {
            const uint8_t slot = cfg_slot_for_row((uint8_t)(menu->cursor - 1));
            const bool is_active =
                host->connected && host->active_slot == slot;
            if (host->slot_has_bond[slot] && !is_active) {
                action.kind = CFG_ACTION_CONNECT_SLOT;
                action.slot = slot;
                menu->screen = CFG_OFF;
                hid_report_clear(report);
            }
        }
    } else if (menu->screen == CFG_RESET) {
        const uint8_t slot = cfg_slot_for_row(menu->cursor);
        if (host->slot_has_bond[slot]) {
            action.kind = CFG_ACTION_RESET_SLOT;
            action.slot = slot;
        }
    } else if (menu->screen == CFG_MACMODE) {
        // 画面は残す（BLE reset と同様、連続で複数スロットを切り替えられる）。
        const uint8_t slot = cfg_slot_for_row(menu->cursor);
        action.kind = CFG_ACTION_SET_MAC_MODE;
        action.slot = slot;
        action.value = !host->slot_mac_mode[slot];
    } else if (menu->screen == CFG_SWAP) {
        menu->swap_from_slot = cfg_slot_for_row(menu->cursor);
        menu->screen = CFG_SWAP_TARGET;
        menu->cursor = 0;
    } else if (menu->screen == CFG_SWAP_TARGET) {
        const uint8_t slot = cfg_slot_for_row(menu->cursor);
        if (slot != menu->swap_from_slot) {
            action.kind = CFG_ACTION_SWAP_SLOTS;
            action.slot = menu->swap_from_slot;
            action.slot_b = slot;
            // 画面は閉じず入れ替え元選択に戻す（連続で複数組を入れ替えられる）。
            menu->screen = CFG_SWAP;
            menu->cursor = 0;
        }
    } else if (menu->screen == CFG_BOOT) {
        action.kind = CFG_ACTION_REBOOT_BOOTLOADER;
    }
    return action;
}

cfg_action_t config_menu_update(config_menu_t *menu,
                                const cfg_host_state_t *host,
                                keyboard_bitmap_report_t *report) {
    cfg_action_t action = {.kind = CFG_ACTION_NONE};

    if (menu->screen == CFG_OFF) {
        return action;
    }
    if (menu->screen == CFG_EDIT) {
        return config_menu_update_edit(menu, report);
    }

    const bool up = hid_report_get_key(report, KC_UP);
    const bool dn = hid_report_get_key(report, KC_DOWN);
    const bool ent = hid_report_get_key(report, KC_ENTER);
    const bool esc = hid_report_get_key(report, KC_ESC);
    const uint8_t count = cfg_item_count(menu->screen);

    if (count > 0) {
        if (up && !menu->nav_prev_up)
            menu->cursor = (uint8_t)((menu->cursor + count - 1) % count);
        if (dn && !menu->nav_prev_down)
            menu->cursor = (uint8_t)((menu->cursor + 1) % count);
    }
    if (esc && !menu->nav_prev_esc) {
        if (menu->screen == CFG_TOP) {
            menu->screen = CFG_OFF;
            // 終了時に押下中の Esc 等がホストへ漏れないよう解放
            hid_report_clear(report);
        } else {
            menu->screen = cfg_parent_screen(menu->screen);
            menu->cursor = 0;
        }
    }
    // Esc と Enter が同一スキャンで同時に立ち上がることがある。Esc で抜けた後に Enter を処理すると CFG_OFF が CFG_BOOT 扱いになり、意図せずブートローダへ再起動してしまう。
    if (ent && !menu->nav_prev_enter && menu->screen != CFG_OFF) {
        action = config_menu_confirm(menu, host, report);
    }
    menu->nav_prev_up = up;
    menu->nav_prev_down = dn;
    menu->nav_prev_enter = ent;
    menu->nav_prev_esc = esc;
    return action;
}
