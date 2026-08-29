#ifndef EHHHKB2_POWER_MONITOR_H
#define EHHHKB2_POWER_MONITOR_H

#include <stddef.h>
#include <stdint.h>

void power_monitor_init(void);

// 100ms 周期でバッテリー電圧をサンプリングする（Core 1 ループから呼ぶ）
void power_monitor_update(void);

// GPIO28 (1/2 分圧): バッテリー電圧 [mV]（最後の update() 時の値）
uint16_t power_monitor_get_battery_voltage_mv(void);

// 電圧を Li-Po の放電曲線で 0〜100% に換算する。充電中は充電電圧を見るため実際の残量より高めに出る。
uint8_t power_monitor_get_battery_percent(void);

#endif  // EHHHKB2_POWER_MONITOR_H
