#include "ui/screen.h"

#include <stdio.h>
#include <string.h>

#include "display/ssd1306.h"
#include "pico/stdlib.h"
#include "power/power_monitor.h"

#define DISPLAY_REFRESH_MS 250

// バッテリー残量アイコンのしきい値 [%]。Battery 画面や BLE 通知と同じ power_monitor_get_battery_percent() を参照するため、表示は必ず一致する。
#define BATTERY_PCT_FULL 75
#define BATTERY_PCT_HIGH 50
#define BATTERY_PCT_LOW 25

// LED インジケータは 3 文字ずつ固定位置に置き、消灯中のものは描かない。
#define STATUS_LINE_Y 24
#define LED_X_NUM 0
#define LED_X_CAPS (4 * FONT_WIDTH)
#define LED_X_SCROLL (8 * FONT_WIDTH)

// USB は usb_hid_get_keyboard_leds()、BLE は ble_hid_get_keyboard_leds()（HIDS の Output レポート経由）から、呼び出し側がまとめて leds へ詰める。
static void draw_led_indicators(const uint8_t leds) {
    if (leds & HID_LED_NUM_LOCK)
        ssd1306_draw_string(LED_X_NUM, STATUS_LINE_Y, "NUM", true);
    if (leds & HID_LED_CAPS_LOCK)
        ssd1306_draw_string(LED_X_CAPS, STATUS_LINE_Y, "CAP", true);
    if (leds & HID_LED_SCROLL_LOCK)
        ssd1306_draw_string(LED_X_SCROLL, STATUS_LINE_Y, "SCR", true);
}

static void draw_booting_screen(void) {
    const char *msg = "Booting...";
    const uint8_t len = (uint8_t)strlen(msg);
    const uint8_t x = (uint8_t)((SSD1306_WIDTH - len * FONT_WIDTH) / 2);
    const uint8_t y = (uint8_t)((SSD1306_HEIGHT - FONT_HEIGHT) / 2);
    ssd1306_draw_string(x, y, msg, true);
}

static void draw_battery_screen(void) {
    const uint16_t mv = power_monitor_get_battery_voltage_mv();
    const uint8_t pct = power_monitor_get_battery_percent();
    char buf[CFG_LINE_MAX];
    ssd1306_draw_string(0, 0, "Battery", true);
    snprintf(buf, sizeof(buf), "%u.%02u V  %u%%", mv / 1000, (mv % 1000) / 10,
             (unsigned)pct);
    ssd1306_draw_string(0, 12, buf, true);
}

// 内部スロット番号（0始まり）を表示用の BT 番号へ変換する。基板の配線都合で内部順序と実物のラベルが逆になっているため、表示側だけ反転させる。
static uint8_t slot_display_number(const uint8_t slot) {
    return (uint8_t)(BLE_HOST_SLOTS - slot);
}

static void draw_connection_status(const ui_state_t *s) {
    const uint8_t slot = s->host.active_slot;
    char buf[BLE_HOST_NAME_MAX + 1];
    if (s->bt_disabled) {
        snprintf(buf, sizeof(buf), "USB");
    } else if (s->host.connected && slot < BLE_HOST_SLOTS) {
        if (s->host.slot_name[slot][0])
            snprintf(buf, sizeof(buf), "%s", s->host.slot_name[slot]);
        else
            snprintf(buf, sizeof(buf), "BT%u",
                    (unsigned)slot_display_number(slot));
    } else if (s->pairing) {
        snprintf(buf, sizeof(buf), "PAIR");
    } else {
        snprintf(buf, sizeof(buf), "---");
    }
    ssd1306_draw_string(0, 0, buf, true);
}

// 右端は充電中なら矢印、非充電なら残量アイコン。USB 接続中はその左に USB アイコンを出す。TinyUSB は Core 0 が排他的に触るため接続状態は引数で受ける。
static void draw_battery_icon(const bool usb_connected) {
    const uint8_t bx = SSD1306_WIDTH - FONT_WIDTH;

    if (usb_connected) {
        ssd1306_draw_char(bx, 0, GLYPH_ARROW, true);
        ssd1306_draw_char(bx - FONT_WIDTH, 0, GLYPH_USB, true);
    } else {
        const uint8_t pct = power_monitor_get_battery_percent();
        char glyph;
        if (pct >= BATTERY_PCT_FULL)
            glyph = GLYPH_BATT_5;
        else if (pct >= BATTERY_PCT_HIGH)
            glyph = GLYPH_BATT_4;
        else if (pct >= BATTERY_PCT_LOW)
            glyph = GLYPH_BATT_2;
        else
            glyph = GLYPH_BATT_0;
        ssd1306_draw_char(bx, 0, glyph, true);
    }
}

// 接続中スロットが Mac モード ON のときだけ、電池アイコンの左に表示する。
static void draw_mac_mode_indicator(const ui_state_t *s) {
    const uint8_t slot = s->host.active_slot;
    if (s->bt_disabled || !s->host.connected || slot >= BLE_HOST_SLOTS) return;
    if (!s->host.slot_mac_mode[slot]) return;
    ssd1306_draw_char(SSD1306_WIDTH - 3 * FONT_WIDTH, 0, 'M', true);
}

static void slot_label(char *out, size_t n, int slot,
                       const char names[][BLE_HOST_NAME_MAX + 1]) {
    if (names[slot][0])
        snprintf(out, n, "%s", names[slot]);
    else
        snprintf(out, n, "BT%d", slot_display_number((uint8_t)slot));
}

// スロット行の label/struck(未ペアリング)/conn(接続中) をまとめて埋める。offset は書き込み開始インデックス（CFG_CONNECT は先頭に USB 行があるため 1、CFG_RESET は 0）。
static void fill_slot_rows(char texts[][CFG_LABEL_MAX], bool struck[],
                           bool conn[], const int offset, const ui_state_t *s,
                           const char names[][BLE_HOST_NAME_MAX + 1]) {
    for (int i = 0; i < BLE_HOST_SLOTS; ++i) {
        const uint8_t slot = cfg_slot_for_row((uint8_t)i);
        const int idx = offset + i;
        slot_label(texts[idx], CFG_LABEL_MAX, slot, names);
        struck[idx] = !s->host.slot_has_bond[slot];
        conn[idx] = s->host.connected && s->host.active_slot == slot;
    }
}

// 128x32 / 6x8 フォントで最大 4 行。超過分はカーソルに追従してスクロールする。
static void draw_config_menu(const ui_state_t *s) {
    const cfg_screen_t screen = s->cfg_screen;
    const uint8_t cursor = s->cfg_cursor;
    const char (*names)[BLE_HOST_NAME_MAX + 1] = s->host.slot_name;

    if (screen == CFG_BATTERY) {
        draw_battery_screen();
        return;
    }

    if (screen == CFG_EDIT) {
        char hdr[CFG_LINE_MAX];
        snprintf(hdr, sizeof(hdr), "Rename BT%d",
                slot_display_number(s->cfg_rename_slot));
        ssd1306_draw_string(0, 0, hdr, true);
        char line[CFG_LINE_MAX];
        snprintf(line, sizeof(line), "%s_", s->cfg_edit);  // 末尾に入力カーソル
        ssd1306_draw_string(0, 12, line, true);
        ssd1306_draw_string(0, 24, "Ent:OK Esc:X", true);
        return;
    }

    char texts[CFG_ITEM_MAX][CFG_LABEL_MAX];
    bool struck[CFG_ITEM_MAX] = {0};    // 取り消し線（未ペアリング）
    bool conn[CFG_ITEM_MAX] = {0};      // 接続中マーク "(C)"
    bool mac_on[CFG_ITEM_MAX] = {0};    // Mac モード ON マーク
    bool from_mark[CFG_ITEM_MAX] = {0}; // 入れ替え元マーク "(F)"（CFG_SWAP_TARGET のみ）
    const int count = (int)cfg_item_count(screen);

    switch (screen) {
        case CFG_TOP:
            snprintf(texts[CFG_TOP_CONNECT], CFG_LABEL_MAX, "Connect");
            snprintf(texts[CFG_TOP_PAIR], CFG_LABEL_MAX,
                    s->pairing ? "CancelPair" : "Pairing");
            snprintf(texts[CFG_TOP_MACMODE], CFG_LABEL_MAX, "Mac mode");
            snprintf(texts[CFG_TOP_RENAME], CFG_LABEL_MAX, "BLE rename");
            snprintf(texts[CFG_TOP_SWAP], CFG_LABEL_MAX, "BLE swap");
            snprintf(texts[CFG_TOP_RESET], CFG_LABEL_MAX, "BLE reset");
            snprintf(texts[CFG_TOP_MISC], CFG_LABEL_MAX, "Misc");
            break;
        case CFG_MISC:
            snprintf(texts[CFG_MISC_BATTERY], CFG_LABEL_MAX, "Battery");
            snprintf(texts[CFG_MISC_DISPLAY_OFF], CFG_LABEL_MAX,
                    s->display_forced_off ? "Display On" : "Display Off");
            snprintf(texts[CFG_MISC_QUIET_MODE], CFG_LABEL_MAX,
                    s->quiet_mode_on ? "Quiet Off" : "Quiet On");
            snprintf(texts[CFG_MISC_BOOT], CFG_LABEL_MAX, "Boot mode");
            break;
        case CFG_CONNECT:
            snprintf(texts[0], CFG_LABEL_MAX, "USB");
            conn[0] = s->bt_disabled;
            fill_slot_rows(texts, struck, conn, 1, s, names);
            break;
        case CFG_RESET:
            fill_slot_rows(texts, struck, conn, 0, s, names);
            break;
        case CFG_RENAME:
            for (int i = 0; i < BLE_HOST_SLOTS; ++i) {
                const uint8_t slot = cfg_slot_for_row((uint8_t)i);
                if (names[slot][0])
                    snprintf(texts[i], CFG_LABEL_MAX, "BT%d %s",
                             slot_display_number(slot), names[slot]);
                else
                    snprintf(texts[i], CFG_LABEL_MAX, "BT%d",
                             slot_display_number(slot));
            }
            break;
        case CFG_MACMODE:
            for (int i = 0; i < BLE_HOST_SLOTS; ++i) {
                const uint8_t slot = cfg_slot_for_row((uint8_t)i);
                slot_label(texts[i], CFG_LABEL_MAX, slot, names);
                mac_on[i] = s->host.slot_mac_mode[slot];
            }
            break;
        case CFG_SWAP:
            fill_slot_rows(texts, struck, conn, 0, s, names);
            break;
        case CFG_SWAP_TARGET:
            fill_slot_rows(texts, struck, conn, 0, s, names);
            for (int i = 0; i < BLE_HOST_SLOTS; ++i) {
                const uint8_t slot = cfg_slot_for_row((uint8_t)i);
                from_mark[i] = slot == s->cfg_swap_from_slot;
            }
            break;
        default:  // CFG_BOOT
            snprintf(texts[0], CFG_LABEL_MAX, "continue?");
            break;
    }

    // カーソルが見えるようにスクロール窓を決める
    int first = 0;
    if (count > CFG_VISIBLE_ROWS) {
        first = (int)cursor - 1;
        if (first < 0) first = 0;
        if (first > count - CFG_VISIBLE_ROWS) first = count - CFG_VISIBLE_ROWS;
    }

    for (int row = 0; row < CFG_VISIBLE_ROWS && (first + row) < count; ++row) {
        const int idx = first + row;
        char line[CFG_LINE_MAX];
        snprintf(line, sizeof(line), "%c %s", (idx == (int)cursor) ? '>' : ' ',
                 texts[idx]);
        const uint8_t y = (uint8_t)(row * 8);
        ssd1306_draw_string(0, y, line, true);
        if (struck[idx]) {
            const int len = (int)strlen(line);
            ssd1306_draw_hline(2 * FONT_WIDTH, y + 3,
                               (uint8_t)((len - 2) * FONT_WIDTH), true);
        }
        if (screen == CFG_MACMODE) {
            const uint8_t cx = SSD1306_WIDTH - 3 * FONT_WIDTH;
            ssd1306_draw_string(cx, y, mac_on[idx] ? " ON" : "OFF", true);
        } else if (screen == CFG_SWAP_TARGET && from_mark[idx]) {
            const uint8_t cx = SSD1306_WIDTH - 3 * FONT_WIDTH;
            ssd1306_draw_string(cx, y, "(F)", true);
        } else if (conn[idx]) {
            const uint8_t cx = SSD1306_WIDTH - 3 * FONT_WIDTH;
            ssd1306_draw_string(cx, y, "(C)", true);
        }
    }
}

static void draw_frame(const ui_state_t *s) {
    ssd1306_clear_buffer();
    if (s->booting) {
        draw_booting_screen();
    } else if (s->cfg_screen != CFG_OFF) {
        draw_config_menu(s);
    } else {
        draw_connection_status(s);
        draw_mac_mode_indicator(s);
        draw_battery_icon(s->usb_connected);
        if (s->has_code) {
            char code_str[16];
            snprintf(code_str, sizeof(code_str), "Code:%06lu",
                     (unsigned long)s->code);
            ssd1306_draw_string(0, 12, code_str, true);
        }
        draw_led_indicators(s->leds);
    }
    ssd1306_display();
}

void screen_task(const ui_state_t *s) {
    static absolute_time_t next_update = {0};
    static bool panel_on = true;  // ssd1306_init() 後はパネル点灯状態
    static cfg_screen_t prev_cfg_screen = CFG_OFF;
    static uint8_t prev_cfg_cursor = 0;
    static char prev_cfg_edit[BLE_HOST_NAME_MAX + 1] = {0};
    static uint8_t prev_leds = 0;
    static bool prev_booting = false;

    // メニュー操作と LED 状態は定期更新を待たずに即再描画する（Caps Lock などの反応を遅らせない）。Booting 表示も再起動までの短い猶予しかないため同様に即時反映する。
    if (s->cfg_screen != prev_cfg_screen || s->cfg_cursor != prev_cfg_cursor ||
        s->leds != prev_leds || s->booting != prev_booting ||
        strcmp(s->cfg_edit, prev_cfg_edit) != 0) {
        next_update = (absolute_time_t){0};
        prev_cfg_screen = s->cfg_screen;
        prev_cfg_cursor = s->cfg_cursor;
        prev_leds = s->leds;
        prev_booting = s->booting;
        memcpy(prev_cfg_edit, s->cfg_edit, sizeof(prev_cfg_edit));
    }

    if (!s->display_on) {
        if (panel_on) {
            ssd1306_power_on(false);
            panel_on = false;
        }
        return;
    }

    if (!panel_on) {
        ssd1306_power_on(true);
        panel_on = true;
        next_update = (absolute_time_t){0};
    }
    if (time_reached(next_update)) {
        draw_frame(s);
        next_update = make_timeout_time_ms(DISPLAY_REFRESH_MS);
    }
}
