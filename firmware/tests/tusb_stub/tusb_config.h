#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

// ホスト単体テスト用の最小 TinyUSB 設定。
//
// hid/keycode.h は HID_KEY_* を得るために TinyUSB の class/hid/hid.h を
// 取り込むが、その先の tusb_option.h が tusb_config.h を要求する。
// 実機用の usb/tusb_config.h は RP2350 のレジスタ定義に依存するため、
// ホストでは代わりにこのスタブを include パスに置く。
// デバイススタックは一切初期化しないので、ここではターゲット無指定でよい。

#define CFG_TUSB_MCU OPT_MCU_NONE
#define CFG_TUSB_OS OPT_OS_NONE

#endif  // _TUSB_CONFIG_H_
