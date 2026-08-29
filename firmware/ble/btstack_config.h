#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

#define HAVE_EMBEDDED_TIME_MS
#define HAVE_BTSTACK_STDIN 0
#define HAVE_MALLOC

#define ENABLE_LOG_INFO
#define ENABLE_PRINTF_HEXDUMP
#define ENABLE_LE_PERIPHERAL
#define ENABLE_LE_SECURE_CONNECTIONS
// ホスト切り替え時にホワイトリストフィルタで対象デバイスのみ接続させるため、
// コントローラのアドレス解決（RPA→アイデンティティ）を有効化する。
// gap_load_resolving_list_from_le_device_db() はこのフラグで有効になる。
#define ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION

#define HCI_ACL_PAYLOAD_SIZE 1691
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 4
#define HCI_OUTGOING_PRE_BUFFER_SIZE 4
#define MAX_NR_GATT_CLIENTS 1
#define MAX_NR_HCI_CONNECTIONS 1
#define MAX_NR_L2CAP_CHANNELS 3
#define MAX_NR_LE_DEVICE_DB_ENTRIES 6
#define MAX_NR_SM_LOOKUP_ENTRIES 3
#define MAX_NR_WHITELIST_ENTRIES 6
#define NVM_NUM_DEVICE_DB_ENTRIES 6

#endif
