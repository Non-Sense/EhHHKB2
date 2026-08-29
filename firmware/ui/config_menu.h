#ifndef EHHHKB2_CONFIG_MENU_H
#define EHHHKB2_CONFIG_MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "ble/ble_hid.h"
#include "hid/hid_report.h"

// コンフィグメニュー（FN2+END で起動）の状態機械。ハードウェアには一切触らず、入力はキーレポートと cfg_host_state_t のスナップショットだけ、実行すべき副作用は cfg_action_t として呼び出し側へ返す（単体テスト可能にするため）。

typedef enum {
    CFG_OFF = 0,  // メニュー非表示（通常動作）
    CFG_TOP,      // トップ: Connect / Pairing / Mac mode / BLE rename / BLE swap / BLE reset / Misc
    CFG_CONNECT,  // 接続先選択: BT1〜6 / USB
    CFG_RESET,    // ボンド削除: BT1〜6
    CFG_RENAME,   // 名前変更するスロット選択: BT1〜6
    CFG_EDIT,     // 名前入力
    CFG_MACMODE,  // Mac モード切替するスロット選択: BT1〜6
    CFG_SWAP,        // 入れ替え元スロット選択: BT1〜6
    CFG_SWAP_TARGET, // 入れ替え先スロット選択: BT1〜6
    CFG_MISC,     // その他: Battery / Display off / Quiet mode / Boot mode
    CFG_BATTERY,  // バッテリー電圧の表示（リストではない）
    CFG_BOOT,     // ブート確認: continue?
} cfg_screen_t;

// CFG_TOP の項目。
enum cfg_top_item {
    CFG_TOP_CONNECT = 0,
    CFG_TOP_PAIR,
    CFG_TOP_MACMODE,
    CFG_TOP_RENAME,
    CFG_TOP_SWAP,
    CFG_TOP_RESET,
    CFG_TOP_MISC,
    CFG_TOP_COUNT,
};

// CFG_MISC の項目。CFG_MISC_BOOT は破壊的操作なので末尾に置く。
enum cfg_misc_item {
    CFG_MISC_BATTERY = 0,
    CFG_MISC_DISPLAY_OFF,
    CFG_MISC_QUIET_MODE,
    CFG_MISC_BOOT,
    CFG_MISC_COUNT,
};

// ---- レイアウト（描画側と共有）----
#define CFG_VISIBLE_ROWS 4  // 画面に収まる行数（128x32 / 6x8 フォント）
#define CFG_LABEL_MAX 16    // 1 項目のラベル長（終端含む）
#define CFG_LINE_MAX 22     // 1 行分の描画バッファ長（"> " + ラベル + 終端）
// 一覧画面の最大項目数（CFG_TOP・CFG_CONNECT・CFG_MISC のうち最大のもの）
#define CFG_MAX2(a, b) ((a) > (b) ? (a) : (b))
#define CFG_ITEM_MAX \
    CFG_MAX2(CFG_MAX2(BLE_HOST_SLOTS + 1, CFG_TOP_COUNT), CFG_MISC_COUNT)

// Connect / BLE reset / Rename 画面のカーソル位置（表示行、0始まり = 最上段）を実際の BLE ホストスロット番号へ変換する。基板の配線都合で内部のスロット番号は逆順（表示 BT1 = 内部スロット BLE_HOST_SLOTS-1）になっているため、一覧は表示上 BT1→BT4 の昇順に並ぶよう、この変換を通してスロットを参照する。config_menu.c（選択操作）と ui/screen.c（描画）の両方から使う。
static inline uint8_t cfg_slot_for_row(const uint8_t row) {
    return (uint8_t)(BLE_HOST_SLOTS - 1 - row);
}

// メニューが参照する BLE 側の状態。呼び出し側が毎ループ埋める。slot_has_bond はメニュー表示中のみ意味を持つ（非表示中は問い合わせない）。
typedef struct {
    bool connected;
    uint8_t active_slot;
    bool slot_has_bond[BLE_HOST_SLOTS];
    char slot_name[BLE_HOST_SLOTS][BLE_HOST_NAME_MAX + 1];
    bool slot_mac_mode[BLE_HOST_SLOTS];
} cfg_host_state_t;

// メニューが呼び出し側へ要求する副作用
typedef enum {
    CFG_ACTION_NONE = 0,
    CFG_ACTION_CONNECT_SLOT,
    CFG_ACTION_DISABLE_BT,
    CFG_ACTION_RESET_SLOT,
    CFG_ACTION_SET_SLOT_NAME,
    CFG_ACTION_SET_MAC_MODE,
    CFG_ACTION_TOGGLE_PAIRING,
    CFG_ACTION_SWAP_SLOTS,
    CFG_ACTION_TOGGLE_DISPLAY,
    CFG_ACTION_TOGGLE_QUIET_MODE,
    CFG_ACTION_REBOOT_BOOTLOADER,
} cfg_action_kind_t;

typedef struct {
    cfg_action_kind_t kind;
    uint8_t slot;      // CONNECT_SLOT / RESET_SLOT / SET_SLOT_NAME / SET_MAC_MODE / SWAP_SLOTS(入れ替え元) で有効
    uint8_t slot_b;    // SWAP_SLOTS で有効（入れ替え先）
    const char *name;  // SET_SLOT_NAME で有効（メニュー内バッファを指す）
    bool value;        // SET_MAC_MODE で有効（true = Mac モード ON）
} cfg_action_t;

typedef struct {
    cfg_screen_t screen;
    uint8_t cursor;
    uint8_t rename_slot;
    uint8_t swap_from_slot;
    char edit_buf[BLE_HOST_NAME_MAX + 1];
    uint8_t edit_len;
    // 立ち上がりエッジ検出用の前回状態
    bool nav_prev_up, nav_prev_down, nav_prev_enter, nav_prev_esc;
    keyboard_bitmap_report_t edit_prev;
} config_menu_t;

void config_menu_init(config_menu_t *menu);

// メニュー表示中か（true の間は HID 送信を止める）
bool config_menu_is_active(const config_menu_t *menu);

// メニューを開く（KC_CFG 押下時）。押下中のキーが漏れないよう report を消す。
void config_menu_open(config_menu_t *menu, keyboard_bitmap_report_t *report);

// キーレポートを処理して画面遷移を進め、実行すべき副作用を返す。終了・確定時には押下中のキーがホストへ漏れないよう report を消す。
cfg_action_t config_menu_update(config_menu_t *menu,
                                const cfg_host_state_t *host,
                                keyboard_bitmap_report_t *report);

// 各画面の項目数（CFG_OFF / CFG_EDIT はリストでないため 0）。CFG_BATTERY / CFG_BOOT はリストではないが、カーソル移動を無害にするため 1。
uint8_t cfg_item_count(cfg_screen_t screen);

#endif  // EHHHKB2_CONFIG_MENU_H
