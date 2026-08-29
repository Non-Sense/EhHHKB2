#include "ble/ble_hid.h"

#include <string.h>

#include "ble/att_server.h"
#include "ble/gatt-service/battery_service_server.h"
#include "ble/gatt-service/device_information_service_server.h"
#include "ble/le_device_db.h"
#include "ble/le_device_db_tlv.h"
#define HID_REPORT_TYPE_INPUT BTSTACK_HID_REPORT_TYPE_INPUT
#define HID_REPORT_TYPE_OUTPUT BTSTACK_HID_REPORT_TYPE_OUTPUT
#define HID_REPORT_TYPE_FEATURE BTSTACK_HID_REPORT_TYPE_FEATURE
#define hid_report_type_t btstack_hid_report_type_t
#include "ble/gatt-service/hids_device.h"
#undef hid_report_type_t
#undef HID_REPORT_TYPE_FEATURE
#undef HID_REPORT_TYPE_OUTPUT
#undef HID_REPORT_TYPE_INPUT
#include "ble/sm.h"
#include "bluetooth.h"
#include "bluetooth_data_types.h"
#include "btstack_event.h"
#include "btstack_tlv.h"
// ehhhkb2.gatt から pico_btstack_make_gatt_header が生成する GATT データベース（profile_data）。同名の ehhhkb2.c とは無関係。
#include "ehhhkb2.h"
#include "gap.h"
#include "hci.h"
#include "hid/hid_report.h"
#include "l2cap.h"
#include "pico/time.h"

#define BLE_HID_SERVICE_UUID 0x1812

// レポートディスクリプタと ehhhkb2.gatt の REPORT_REFERENCE で宣言している Report ID（USB 側の REPORT_ID_* と同じ番号）。
#define BLE_REPORT_ID_KEYBOARD 1
#define BLE_REPORT_ID_CONSUMER 2

// ehhhkb2.gatt が宣言する Report 特性の数（Input 2 本 + Output 1 本）
#define BLE_HID_REPORT_COUNT 3

// バッテリー残量の通知間隔の下限。ADC のばらつきで 1% 単位の変化が続いてもホストへ通知を撒き散らさないようにする。
#define BLE_BATTERY_MIN_INTERVAL_MS 60000

// アドバタイズ間隔（0.625ms 単位）= 30ms 〜 60ms
#define BLE_ADV_INTERVAL_MIN 0x0030
#define BLE_ADV_INTERVAL_MAX 0x0060

// gap_advertisements_set_params の filter policy / チャネルマップ
#define BLE_ADV_FILTER_ANY 0x00
#define BLE_ADV_FILTER_WHITELIST 0x03
#define BLE_ADV_CHANNEL_ALL 0x07

// clang-format off
static const uint8_t hid_descriptor_keyboard_boot_mode[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)

    // Report ID 1。hids.gatt の Report Reference が id=1 を宣言しているため、レポートマップ側にも一致する Report ID が必要。無いとホストが先頭1バイトを予約してフィールドが1バイトずれ、装飾キーが届かなくなる（送信値自体に Report ID バイトは含めない）。
    0x85, 0x01,  // Report ID (1)

    0x75, 0x01,  // Report Size (1 bit)
    0x95, 0x08,  // Report Count (8 modifiers)
    0x05, 0x07,  // Usage Page (Keyboard/Keypad)
    0x19, 0xE0,  // Usage Minimum (Left Control)
    0x29, 0xE7,  // Usage Maximum (Right GUI)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x81, 0x02,  // Input (Data, Variable, Absolute): modifier bitmap

    0x75, 0x08,  // Report Size (8 bits)
    0x95, 0x01,  // Report Count (1 byte)
    0x81, 0x03,  // Input (Constant, Variable, Absolute): reserved byte

    0x95, 0x05,  // Report Count (5 LEDs)
    0x75, 0x01,  // Report Size (1 bit)
    0x05, 0x08,  // Usage Page (LEDs)
    0x19, 0x01,  // Usage Minimum (Num Lock)
    0x29, 0x05,  // Usage Maximum (Kana)
    0x91, 0x02,  // Output (Data, Variable, Absolute): LED states

    0x95, 0x01,  // Report Count (1 field)
    0x75, 0x03,  // Report Size (3 bits)
    0x91, 0x03,  // Output (Constant, Variable, Absolute): LED padding

    0x95, 0x80,  // Report Count (128 keys)
    0x75, 0x01,  // Report Size (1 bit per key)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x05, 0x07,  // Usage Page (Keyboard/Keypad)
    0x19, 0x00,  // Usage Minimum (Reserved / no event)
    0x29, 0x7F,  // Usage Maximum (Keyboard usage 0x7F)
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
    0x85, 0x02,        // Report ID (2)
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

// clang-format off
static const uint8_t adv_data[] = {
    0x02,
    BLUETOOTH_DATA_TYPE_FLAGS,
    0x06,
    0x08,
    BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME,
    'E', 'h', 'H', 'H', 'K', 'B', '2',
    0x03,
    BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS,
    BLE_HID_SERVICE_UUID & 0xFF,
    BLE_HID_SERVICE_UUID >> 8,
    0x03,
    BLUETOOTH_DATA_TYPE_APPEARANCE,
    0xC1,
    0x03,
};
// clang-format on

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static hci_con_handle_t ble_con_handle = HCI_CON_HANDLE_INVALID;
static uint8_t ble_protocol_mode = 1;
static bool ble_input_ready = false;
static bool ble_consumer_ready = false;
static bool ble_can_send_pending = false;
static keyboard_bitmap_report_t last_ble_report;
static keyboard_bitmap_report_t pending_report;
static uint16_t last_ble_consumer = 0;
static uint16_t pending_consumer = 0;
static hids_device_report_t hid_reports[BLE_HID_REPORT_COUNT];
static bool ble_connected = false;
static bool connection_complete_flag = false;
static uint32_t confirmation_code = 0;
static bool confirmation_code_available = false;

// ホストが Output レポートで通知してきた LED 状態（HID_LED_* のビット和）
static uint8_t ble_leds = 0;

static uint8_t battery_level_sent = 0xFF;  // 0xFF = 未送信
static absolute_time_t battery_next_update = {0};

static uint8_t target_slot = 0;              // 次に接続したいスロット
static uint8_t active_slot = BLE_SLOT_NONE;  // 現在接続中のスロット
static bool pairing_requested = false;

// ボンド全削除後は再アドバタイズせず待機する（切断完了ハンドラ用ワンショット）
static bool clear_bonds_idle = false;

// BT 無効化中（KC_USB）。true の間は接続せず広告も出さない（USB 接続のみ）。
static bool bt_disabled = false;

// コントローラのリゾルビングリストへボンド済み IRK を読み込めたか。読み込めていれば RPA を使うホストでもホワイトリストフィルタが機能する。
static bool resolving_list_loaded = false;

// スロットごとのユーザー設定名（TLV フラッシュに永続化）
static char slot_name[BLE_HOST_SLOTS][BLE_HOST_NAME_MAX + 1];
#define SLOT_NAME_TAG(slot) ((uint32_t)0x4e4d0000u | (uint32_t)(slot))

// スロットごとの Mac モード設定（TLV フラッシュに永続化）
static bool mac_mode[BLE_HOST_SLOTS];
#define MAC_MODE_TAG(slot) ((uint32_t)0x4d4d0000u | (uint32_t)(slot))

static void ble_hid_load_settings(void) {
    const btstack_tlv_t *tlv = NULL;
    void *ctx = NULL;
    btstack_tlv_get_instance(&tlv, &ctx);
    for (int i = 0; i < BLE_HOST_SLOTS; ++i) {
        slot_name[i][0] = 0;
        mac_mode[i] = false;
        if (!tlv) continue;
        uint8_t buf[BLE_HOST_NAME_MAX + 1] = {0};
        const int len =
            tlv->get_tag(ctx, SLOT_NAME_TAG(i), buf, BLE_HOST_NAME_MAX);
        if (len > 0 && len <= BLE_HOST_NAME_MAX) {
            memcpy(slot_name[i], buf, (size_t)len);
            slot_name[i][len] = 0;
        }
        uint8_t mac_buf[1] = {0};
        if (tlv->get_tag(ctx, MAC_MODE_TAG(i), mac_buf, sizeof(mac_buf)) > 0) {
            mac_mode[i] = mac_buf[0] != 0;
        }
    }
}

// 現在ホワイトリストに登録しているデバイス（削除用）
static int whitelist_addr_type = -1;
static bd_addr_t whitelist_addr;

static void ble_hid_whitelist_clear(void) {
    if (whitelist_addr_type >= 0) {
        gap_whitelist_remove((bd_addr_type_t)whitelist_addr_type,
                             whitelist_addr);
        whitelist_addr_type = -1;
    }
}

static void ble_hid_apply_advertising(const uint8_t filter_policy) {
    bd_addr_t null_addr = {0};
    gap_advertisements_set_params(BLE_ADV_INTERVAL_MIN, BLE_ADV_INTERVAL_MAX, 0,
                                  0, null_addr, BLE_ADV_CHANNEL_ALL,
                                  filter_policy);
    gap_advertisements_set_data(sizeof(adv_data), (uint8_t *)adv_data);
    gap_advertisements_enable(true);
}

static void ble_hid_start_open_advertising(void) {
    ble_hid_whitelist_clear();
    ble_hid_apply_advertising(BLE_ADV_FILTER_ANY);
}

static void ble_hid_start_advertising_for_slot(const uint8_t slot) {
    ble_hid_whitelist_clear();

    bool use_whitelist = false;
    if (slot < BLE_HOST_SLOTS) {
        // スロット番号＝le_device_db のインデックス。
        int addr_type;
        sm_key_t irk;
        le_device_db_info(slot, &addr_type, whitelist_addr, irk);

        bool valid = false;
        for (int b = 0; b < 6; ++b) {
            if (whitelist_addr[b] != 0) {
                valid = true;
                break;
            }
        }
        if (valid) {
            gap_whitelist_add((bd_addr_type_t)addr_type, whitelist_addr);
            whitelist_addr_type = addr_type;
            use_whitelist = true;
        }
    }

    // ホワイトリストフィルタで対象デバイス以外の接続要求を弾く。無いと直前まで接続していたホストのOSが即座に自動再接続し切り替えが成立しない。RPAを使うホストでもリゾルビングリストにIRKがあれば照合が通るため、未ロード時のみオープン広告へフォールバックする。
    const uint8_t filter_policy = (use_whitelist && resolving_list_loaded)
                                      ? BLE_ADV_FILTER_WHITELIST
                                      : BLE_ADV_FILTER_ANY;
    ble_hid_apply_advertising(filter_policy);
}

// HCI_STATE_WORKING と切断完了ハンドラの両方から呼ばれる共通ロジック。
static void ble_hid_resume_advertising(void) {
    if (bt_disabled) {
        gap_advertisements_enable(false);
    } else if (clear_bonds_idle) {
        clear_bonds_idle = false;
        gap_advertisements_enable(false);
    } else if (pairing_requested) {
        ble_hid_start_open_advertising();
    } else {
        ble_hid_start_advertising_for_slot(target_slot);
    }
}

// pending も last と揃えて消す。last だけ消すと pending が「未送信の変化」として残り続け、Consumer 側の送信要求を食い続ける。
static void ble_hid_reset_connection_state(void) {
    ble_con_handle = HCI_CON_HANDLE_INVALID;
    ble_input_ready = false;
    ble_consumer_ready = false;
    ble_can_send_pending = false;
    ble_connected = false;
    active_slot = BLE_SLOT_NONE;
    hid_report_clear(&last_ble_report);
    hid_report_clear(&pending_report);
    last_ble_consumer = 0;
    pending_consumer = 0;
    ble_leds = 0;
}

// 失敗時は last_ble_report を更新しない（次周回で再送させるため）。
static bool ble_hid_send_now(const keyboard_bitmap_report_t *report) {
    if (ble_con_handle == HCI_CON_HANDLE_INVALID || !ble_input_ready) {
        return false;
    }

    uint8_t status;
    if (ble_protocol_mode == 0) {
        keyboard_boot_report_t boot_report;
        hid_report_to_boot(report, &boot_report);
        status = hids_device_send_boot_keyboard_input_report(
            ble_con_handle, (const uint8_t *)&boot_report, sizeof(boot_report));
    } else {
        // ディスクリプタの入力レポートは modifier(1)+予約(1)+bitmap(16)+上位usage配列(2)=20バイト。構造体は予約バイトを持たないため、そのまま送るとmodifierとbitmapの間の予約バイトが欠落しフィールド境界が1バイトずれて装飾キーが化けるので、ここで予約バイトを挿入して組み立てる。
        uint8_t buf[1 + 1 + KEY_BITMAP_BYTES + KEY_EXTRA_KEYS];
        buf[0] = report->modifier;
        buf[1] = 0;
        memcpy(&buf[2], report->bitmap, KEY_BITMAP_BYTES);
        memcpy(&buf[2 + KEY_BITMAP_BYTES], report->extra, KEY_EXTRA_KEYS);
        status = hids_device_send_input_report_for_id(
            ble_con_handle, BLE_REPORT_ID_KEYBOARD, buf, sizeof(buf));
    }

    if (status != ERROR_CODE_SUCCESS) {
        return false;
    }
    last_ble_report = *report;
    return true;
}

// 失敗時は last_ble_consumer を更新しない（次周回で再送させるため）。
static bool ble_hid_send_consumer_now(const uint16_t usage) {
    if (ble_con_handle == HCI_CON_HANDLE_INVALID || !ble_consumer_ready) {
        return false;
    }
    // ブートプロトコルには Consumer Control が無いので送れない。
    if (ble_protocol_mode == 0) {
        return false;
    }

    const uint8_t buf[2] = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    if (hids_device_send_input_report_for_id(
            ble_con_handle, BLE_REPORT_ID_CONSUMER, buf, sizeof(buf)) !=
        ERROR_CODE_SUCCESS) {
        return false;
    }
    last_ble_consumer = usage;
    return true;
}

// 入力レポート有効化で接続完了を確定する。report/boot 両方の ENABLE イベントから呼ぶことで、ホストが boot プロトコルを使っても PAIR 表示が残らない。
static void ble_hid_mark_connected(void) {
    if (!ble_input_ready || ble_connected) {
        return;
    }
    confirmation_code_available = false;
    ble_connected = true;
    connection_complete_flag = true;
    pairing_requested = false;
    active_slot = target_slot;
}

static void ble_hid_packet_handler(const uint8_t packet_type,
                                   const uint16_t channel, uint8_t *packet,
                                   const uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                // ボンド済み IRK の読み込みは広告開始より前に行う必要がある（ホワイトリストフィルタの前提）。
                resolving_list_loaded =
                    gap_load_resolving_list_from_le_device_db() ==
                    ERROR_CODE_SUCCESS;
                ble_hid_load_settings();
                // 通常起動だけでなく、USB による HCI_POWER_OFF からの手動復帰（HCI_POWER_ON）でもここに来る。
                ble_hid_resume_advertising();
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            ble_hid_reset_connection_state();
            // HID の既定はレポートプロトコル。接続ごとに既定へ戻す（前のホストがブートモードにした状態を引きずらないため）。
            ble_protocol_mode = 1;
            // 切断中（広告停止状態）はリゾルビングリストを安全に更新できる。新規ボンドを反映し、切替時のホワイトリスト照合を確実にする。
            resolving_list_loaded =
                gap_load_resolving_list_from_le_device_db() ==
                ERROR_CODE_SUCCESS;
            ble_hid_resume_advertising();
            break;

        case SM_EVENT_PASSKEY_DISPLAY_NUMBER: {
            uint32_t passkey =
                sm_event_passkey_display_number_get_passkey(packet);
            confirmation_code = passkey;
            confirmation_code_available = true;
        } break;

        case SM_EVENT_JUST_WORKS_REQUEST: {
            hci_con_handle_t handle =
                sm_event_just_works_request_get_handle(packet);
            sm_just_works_confirm(handle);
        } break;

        // 既知デバイスの再接続：どのスロット（＝DBインデックス）かを特定する。
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED: {
            if (pairing_requested) {
                // ペアリング中は DB 登録済み（既知）デバイスを無視する。切断して開放広告を継続し、新規デバイスだけを受け付ける。既知端末を再ペアリングしたい場合は、先にそのスロットを Reset（ボンド削除）してから行う。
                const hci_con_handle_t handle =
                    sm_event_identity_resolving_succeeded_get_handle(packet);
                gap_disconnect(handle);
                break;
            }
            const int device_index =
                sm_event_identity_resolving_succeeded_get_index(packet);
            if (device_index >= 0 && device_index < BLE_HOST_SLOTS) {
                target_slot = (uint8_t)device_index;
            }
        } break;

        // 新規ボンド完了。le_device_db は最初の空きスロットを使い、削除してもインデックスを詰めないため、count から推測せずこの index を使う。
        case SM_EVENT_IDENTITY_CREATED: {
            const int device_index =
                sm_event_identity_created_get_index(packet);
            if (device_index >= 0 && device_index < BLE_HOST_SLOTS) {
                target_slot = (uint8_t)device_index;
            }
        } break;

        case HCI_EVENT_HIDS_META:
            switch (hci_event_hids_meta_get_subevent_code(packet)) {
                // Input レポートは2本（キーボード/Consumer）あり、それぞれの購読で個別にこのイベントが来る。report id で振り分けないと片方の購読で他方の状態を壊す。
                case HIDS_SUBEVENT_INPUT_REPORT_ENABLE: {
                    ble_con_handle =
                        hids_subevent_input_report_enable_get_con_handle(
                            packet);
                    const bool enabled =
                        hids_subevent_input_report_enable_get_enable(packet) !=
                        0;
                    if (hids_subevent_input_report_enable_get_report_id(
                            packet) == BLE_REPORT_ID_CONSUMER) {
                        ble_consumer_ready = enabled;
                        // 接続完了の確定はキーボード側の購読だけで判断する。
                        break;
                    }
                    ble_input_ready = enabled;
                    ble_hid_mark_connected();
                } break;

                case HIDS_SUBEVENT_BOOT_KEYBOARD_INPUT_REPORT_ENABLE:
                    ble_con_handle =
                        hids_subevent_boot_keyboard_input_report_enable_get_con_handle(
                            packet);
                    ble_input_ready =
                        hids_subevent_boot_keyboard_input_report_enable_get_enable(
                            packet) != 0;
                    ble_hid_mark_connected();
                    break;

                case HIDS_SUBEVENT_PROTOCOL_MODE:
                    ble_protocol_mode =
                        hids_subevent_protocol_mode_get_protocol_mode(packet);
                    break;

                // キーボード Output レポート（NumLock/CapsLock/ScrollLock の LED 状態）。ehhhkb2.gatt が Report ID 1 の Output 特性と Boot Keyboard Output 特性の両方を宣言しているため、ホストがどちらの経路で書いてきても BTstack がここへ届けてくれる。
                case HIDS_SUBEVENT_SET_REPORT: {
                    if (hids_subevent_set_report_get_report_type(packet) ==
                            BTSTACK_HID_REPORT_TYPE_OUTPUT &&
                        hids_subevent_set_report_get_report_length(packet) >=
                            1) {
                        ble_leds =
                            hids_subevent_set_report_get_report_data(
                                packet)[0];
                    }
                } break;

                case HIDS_SUBEVENT_CAN_SEND_NOW:
                    ble_can_send_pending = false;
                    // 1イベントで送れる通知は1つ。残った方は次周回の *_if_needed が再要求する。失敗しても last_* は更新されないため同様に再送される。ハンドラ内で再要求すると密ループ化して固着するため避ける。
                    if (!hid_report_equal(&pending_report, &last_ble_report)) {
                        ble_hid_send_now(&pending_report);
                    } else if (pending_consumer != last_ble_consumer) {
                        ble_hid_send_consumer_now(pending_consumer);
                    }
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }
}

void ble_hid_init(void) {
    hid_report_clear(&last_ble_report);
    hid_report_clear(&pending_report);

    l2cap_init();

    sm_init();
    // パスキー要求をしない：入出力なし端末として振る舞い、MITM 保護を要求しないことで Just Works ペアリング（コード入力・表示なし）にする。
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION |
                                       SM_AUTHREQ_BONDING);

    att_server_init(profile_data, NULL, NULL);
    // 実測値は起動直後の ble_hid_update_battery_level() で上書きされる。
    battery_service_server_init(0);
    device_information_service_server_init();
    // hids_device_init() は Report 特性3本ぶんの固定ストレージしか持たず Input を1本しか想定しない。Consumer 用の2本目を扱うため storage 版で初期化する（ehhhkb2.gatt の宣言順に (id, type) が登録される）。
    hids_device_init_with_storage(0, hid_descriptor_keyboard_boot_mode,
                                 sizeof(hid_descriptor_keyboard_boot_mode),
                                 BLE_HID_REPORT_COUNT, hid_reports);

    hci_event_callback_registration.callback = &ble_hid_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sm_event_callback_registration.callback = &ble_hid_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    hids_device_register_packet_handler(ble_hid_packet_handler);
    // 広告開始は BTSTACK_EVENT_STATE / HCI_STATE_WORKING ハンドラで行う。
    hci_power_control(HCI_POWER_ON);
}

// 未送信の変化があれば can_send_now を要求する。キーボードと Consumer のどちらの変化でも同じ1本の要求を共有する。
static void ble_hid_request_send_if_dirty(void) {
    if (ble_con_handle == HCI_CON_HANDLE_INVALID) {
        return;
    }
    const bool keyboard_dirty =
        ble_input_ready && !hid_report_equal(&pending_report, &last_ble_report);
    const bool consumer_dirty =
        ble_consumer_ready && (pending_consumer != last_ble_consumer);
    if (!keyboard_dirty && !consumer_dirty) {
        return;
    }
    if (ble_can_send_pending) {
        return;
    }

    // pending は「要求の前」に立てる。can_send_now コールバックは即送信可能なとき要求関数内で同期的に発火するため、後に立てると false 化された直後に true で上書きし、以降の送信が永久にブロックされる。
    ble_can_send_pending = true;
    if (hids_device_request_can_send_now_event(ble_con_handle) !=
        ERROR_CODE_SUCCESS) {
        ble_can_send_pending = false;  // 次ループで再試行
    }
}

// Ctrl と GUI(Win/Cmd) の modifier ビットを入れ替える。Alt は OS 側が Windows=Alt/macOS=Option と解釈し分けるので不要だが、Ctrl と GUI にはその区別がないため、Mac モードでは物理 Ctrl キーを GUI usage（macOS で Command）として送る。
static uint8_t swap_ctrl_gui(const uint8_t modifier) {
    const uint8_t ctrl_gui_bits = 0x01u | 0x08u | 0x10u | 0x80u;
    uint8_t result = (uint8_t)(modifier & (uint8_t)~ctrl_gui_bits);
    if (modifier & 0x01u) result |= 0x08u;  // LCtrl -> LGUI(Cmd)
    if (modifier & 0x08u) result |= 0x01u;  // LGUI(Win) -> LCtrl
    if (modifier & 0x10u) result |= 0x80u;  // RCtrl -> RGUI
    if (modifier & 0x80u) result |= 0x10u;  // RGUI(Win) -> RCtrl
    return result;
}

void ble_hid_send_report_if_needed(const keyboard_bitmap_report_t *report) {
    if (ble_con_handle == HCI_CON_HANDLE_INVALID || !ble_input_ready) {
        return;
    }

    // can_send_now 待ちの間に状態が変わっても最新（離鍵を含む）が送られる。
    pending_report = *report;
    if (active_slot < BLE_HOST_SLOTS && mac_mode[active_slot]) {
        pending_report.modifier = swap_ctrl_gui(pending_report.modifier);
    }
    ble_hid_request_send_if_dirty();
}

void ble_hid_send_consumer_if_needed(const uint16_t usage) {
    if (ble_con_handle == HCI_CON_HANDLE_INVALID || !ble_consumer_ready) {
        return;
    }

    pending_consumer = usage;
    ble_hid_request_send_if_dirty();
}

void ble_hid_update_battery_level(const uint8_t percent) {
    if (percent == battery_level_sent) {
        return;
    }
    // 変化があるときだけ間隔を判定する。変化なしで下限を消費すると、直後に本当の変化が来ても最大1分遅れる。
    if (!time_reached(battery_next_update)) {
        return;
    }
    battery_next_update = make_timeout_time_ms(BLE_BATTERY_MIN_INTERVAL_MS);
    battery_level_sent = percent;
    // 未接続でも値は保持され、次の接続で読める（購読者がいなければ通知しない）。
    battery_service_server_set_battery_value(percent);
}

bool ble_hid_is_connected(void) { return ble_connected; }

uint8_t ble_hid_get_keyboard_leds(void) { return ble_leds; }

uint32_t ble_hid_get_confirmation_code(void) {
    if (confirmation_code_available) {
        return confirmation_code;
    }
    return 0;
}

inline bool ble_hid_has_confirmation_code(void) {
    return confirmation_code_available;
}

bool ble_hid_is_connection_complete(void) {
    if (connection_complete_flag) {
        connection_complete_flag = false;
        return true;
    }
    return false;
}

void ble_hid_switch_host(const uint8_t slot) {
    if (slot >= BLE_HOST_SLOTS) return;
    const bool was_disabled = bt_disabled;
    bt_disabled = false;
    target_slot = slot;
    if (was_disabled) {
        // HCI 電源 OFF（USB 接続時）からの手動復帰：電源を入れると HCI_STATE_WORKING で target_slot 向けの再接続広告が始まる。
        pairing_requested = false;
        hci_power_control(HCI_POWER_ON);
        return;
    }
    if (ble_con_handle != HCI_CON_HANDLE_INVALID) {
        // DISCONNECTION_COMPLETE でホワイトリスト広告を開始し、旧ホストの即再接続を防ぐ（ホスト間切替の競合回避）。
        gap_disconnect(ble_con_handle);
    } else {
        // 未接続からの切替はオープン広告にする。ホスト側でボンドが削除・再作成されると IRK が変わり、ホワイトリストでは RPA を解決できず接続を弾いてしまうため、再ペアリングと自動再接続の両方を許可する。
        ble_hid_start_open_advertising();
    }
}

uint8_t ble_hid_get_active_slot(void) { return active_slot; }

void ble_hid_clear_all_bonds(void) {
    // le_device_db_remove だけだとコントローラのアドレス解決リストが残り古い端末が解決・再接続できてしまうため、resolving list も再構築する gap_delete_bonding を使う。
    for (int i = 0; i < le_device_db_max_count(); i++) {
        int addr_type = BD_ADDR_TYPE_UNKNOWN;
        bd_addr_t addr;
        le_device_db_info(i, &addr_type, addr, NULL);
        if (addr_type != BD_ADDR_TYPE_UNKNOWN) {
            gap_delete_bonding((bd_addr_type_t)addr_type, addr);
        }
    }
    for (int i = 0; i < BLE_HOST_SLOTS; ++i) {
        ble_hid_set_slot_name((uint8_t)i, "");
        ble_hid_set_mac_mode((uint8_t)i, false);
    }
    target_slot = 0;
    pairing_requested = false;
    if (ble_con_handle != HCI_CON_HANDLE_INVALID) {
        clear_bonds_idle = true;
        gap_disconnect(ble_con_handle);
    } else {
        gap_advertisements_enable(false);
    }
}

void ble_hid_get_slot_name(const uint8_t slot, char *out, const size_t n) {
    if (n == 0) return;
    out[0] = 0;
    if (slot >= BLE_HOST_SLOTS) return;
    size_t i = 0;
    for (; i + 1 < n && slot_name[slot][i]; ++i) out[i] = slot_name[slot][i];
    out[i] = 0;
}

void ble_hid_set_slot_name(const uint8_t slot, const char *name) {
    if (slot >= BLE_HOST_SLOTS) return;
    size_t i = 0;
    for (; i < BLE_HOST_NAME_MAX && name[i]; ++i) slot_name[slot][i] = name[i];
    slot_name[slot][i] = 0;

    const btstack_tlv_t *tlv = NULL;
    void *ctx = NULL;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (!tlv) return;
    if (i == 0) {
        tlv->delete_tag(ctx, SLOT_NAME_TAG(slot));
    } else {
        tlv->store_tag(ctx, SLOT_NAME_TAG(slot),
                       (const uint8_t *)slot_name[slot], (uint32_t)i);
    }
}

bool ble_hid_get_mac_mode(const uint8_t slot) {
    if (slot >= BLE_HOST_SLOTS) return false;
    return mac_mode[slot];
}

void ble_hid_set_mac_mode(const uint8_t slot, const bool enabled) {
    if (slot >= BLE_HOST_SLOTS) return;
    mac_mode[slot] = enabled;

    const btstack_tlv_t *tlv = NULL;
    void *ctx = NULL;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (!tlv) return;
    if (!enabled) {
        tlv->delete_tag(ctx, MAC_MODE_TAG(slot));
    } else {
        const uint8_t val = 1;
        tlv->store_tag(ctx, MAC_MODE_TAG(slot), &val, sizeof(val));
    }
}

bool ble_hid_slot_has_bond(const uint8_t slot) {
    if (slot >= BLE_HOST_SLOTS) return false;
    int addr_type = BD_ADDR_TYPE_UNKNOWN;
    bd_addr_t addr;
    le_device_db_info((int)slot, &addr_type, addr, NULL);
    return addr_type != BD_ADDR_TYPE_UNKNOWN;
}

void ble_hid_reset_slot(const uint8_t slot) {
    if (slot >= BLE_HOST_SLOTS) return;
    int addr_type = BD_ADDR_TYPE_UNKNOWN;
    bd_addr_t addr;
    le_device_db_info((int)slot, &addr_type, addr, NULL);
    if (addr_type == BD_ADDR_TYPE_UNKNOWN) return;  // 未ペアリング

    gap_delete_bonding((bd_addr_type_t)addr_type, addr);
    ble_hid_set_slot_name(slot, "");
    ble_hid_set_mac_mode(slot, false);

    if (slot == active_slot && ble_con_handle != HCI_CON_HANDLE_INVALID) {
        // 接続中スロットを削除：切断する。切断完了ハンドラでリゾルビングリスト再読込と再アドバタイズが行われる。
        gap_disconnect(ble_con_handle);
    } else if (ble_con_handle == HCI_CON_HANDLE_INVALID) {
        // 未接続時のみリゾルビングリストを更新（接続中の更新は避ける）。
        resolving_list_loaded =
            gap_load_resolving_list_from_le_device_db() == ERROR_CODE_SUCCESS;
    }
}

// le_device_db_tlv.c がボンド本体（アドレス/IRK/LTK 等）を保存する際の TLV タグ。同ファイルの le_device_db_tlv_tag_for_index() と一致させる必要がある（le_device_db.h はレコード構造体もインデックス単位の入れ替え手段も公開していないため、フォーマットを解釈せず TLV タグの中身をまるごと入れ替える）。
#define LE_DEVICE_DB_TAG(index) \
    (((uint32_t)'B' << 24) | ((uint32_t)'T' << 16) | ((uint32_t)'D' << 8) | (index))
// le_device_db_entry_t（非公開）を余裕を持って収められるバッファ長
#define LE_DEVICE_DB_ENTRY_BUF_MAX 256

void ble_hid_swap_slots(const uint8_t a, const uint8_t b) {
    if (a >= BLE_HOST_SLOTS || b >= BLE_HOST_SLOTS || a == b) return;

    const btstack_tlv_t *tlv = NULL;
    void *ctx = NULL;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (tlv) {
        uint8_t buf_a[LE_DEVICE_DB_ENTRY_BUF_MAX];
        uint8_t buf_b[LE_DEVICE_DB_ENTRY_BUF_MAX];
        const int len_a =
            tlv->get_tag(ctx, LE_DEVICE_DB_TAG(a), buf_a, sizeof(buf_a));
        const int len_b =
            tlv->get_tag(ctx, LE_DEVICE_DB_TAG(b), buf_b, sizeof(buf_b));
        if (len_a > 0) {
            tlv->store_tag(ctx, LE_DEVICE_DB_TAG(b), buf_a, (uint32_t)len_a);
        } else {
            tlv->delete_tag(ctx, LE_DEVICE_DB_TAG(b));
        }
        if (len_b > 0) {
            tlv->store_tag(ctx, LE_DEVICE_DB_TAG(a), buf_b, (uint32_t)len_b);
        } else {
            tlv->delete_tag(ctx, LE_DEVICE_DB_TAG(a));
        }
        // le_device_db_tlv.c は使用中インデックスを RAM にキャッシュしており、TLV タグを直接書き換えても追随しない。起動時と同じ呼び出しを再度行わせて強制的に再スキャンさせる。
        le_device_db_tlv_configure(tlv, ctx);
    }

    // 名前と Mac モードは通常の setter で入れ替える（RAM キャッシュと TLV の両方を正しく更新してくれる）。
    char name_a[BLE_HOST_NAME_MAX + 1];
    char name_b[BLE_HOST_NAME_MAX + 1];
    ble_hid_get_slot_name(a, name_a, sizeof(name_a));
    ble_hid_get_slot_name(b, name_b, sizeof(name_b));
    ble_hid_set_slot_name(a, name_b);
    ble_hid_set_slot_name(b, name_a);

    const bool mac_a = ble_hid_get_mac_mode(a);
    const bool mac_b = ble_hid_get_mac_mode(b);
    ble_hid_set_mac_mode(a, mac_b);
    ble_hid_set_mac_mode(b, mac_a);

    // 次に advertise/reconnect すべきスロットも中身の移動に合わせて入れ替える
    if (target_slot == a) {
        target_slot = b;
    } else if (target_slot == b) {
        target_slot = a;
    }

    if ((a == active_slot || b == active_slot) &&
        ble_con_handle != HCI_CON_HANDLE_INVALID) {
        // 接続中スロットの中身を入れ替えた：切断する。切断完了ハンドラでリゾルビングリスト再読込と再アドバタイズが行われる。
        gap_disconnect(ble_con_handle);
    } else if (ble_con_handle == HCI_CON_HANDLE_INVALID) {
        // 未接続時のみリゾルビングリストを更新（接続中の更新は避ける）。
        resolving_list_loaded =
            gap_load_resolving_list_from_le_device_db() == ERROR_CODE_SUCCESS;
    }
}

void ble_hid_disable_bt(void) {
    if (bt_disabled) return;  // 多重 HCI_POWER_OFF を避ける
    bt_disabled = true;
    pairing_requested = false;
    // HCI_POWER_OFF では DISCONNECTION_COMPLETE が発火しないことがあるため、接続状態はここで確定的にクリアする。復帰は BT1〜6 / ペアリング操作。
    hci_power_control(HCI_POWER_OFF);
    ble_hid_reset_connection_state();
}

bool ble_hid_is_disabled(void) { return bt_disabled; }

bool ble_hid_is_pairing(void) { return pairing_requested; }

void ble_hid_start_pairing(void) {
    const bool was_disabled = bt_disabled;
    bt_disabled = false;
    pairing_requested = true;
    if (was_disabled) {
        // HCI 電源 OFF（USB 接続時）からの手動復帰：電源を入れると HCI_STATE_WORKING で pairing_requested によりオープン広告が始まる。
        hci_power_control(HCI_POWER_ON);
        return;
    }
    if (ble_con_handle != HCI_CON_HANDLE_INVALID) {
        // 切断後 HCI_EVENT_DISCONNECTION_COMPLETE でオープン広告が始まる
        gap_disconnect(ble_con_handle);
    } else {
        ble_hid_start_open_advertising();
    }
}

void ble_hid_cancel_pairing(void) {
    if (!pairing_requested) return;
    pairing_requested = false;
    if (bt_disabled) return;
    if (ble_con_handle != HCI_CON_HANDLE_INVALID) {
        // 切断後 HCI_EVENT_DISCONNECTION_COMPLETE で target_slot 向けの通常広告（ホワイトリストフィルタ）が始まる
        gap_disconnect(ble_con_handle);
    } else {
        ble_hid_start_advertising_for_slot(target_slot);
    }
}
