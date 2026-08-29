#include "power_monitor.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define USB_SENSE_GPIO 22
#define BATTERY_ADC_GPIO 28
#define BATTERY_ADC_CH 2  // GPIO28 = ADC2
#define VREF_MV 3300
#define ADC_BITS 12

#define POWER_SAMPLE_INTERVAL_MS 100

// 1 回の読みは数 LSB（1 LSB ≒ 1.6mV）ばらつくため、平均してから使う。
#define POWER_ADC_OVERSAMPLE 16

#define BATTERY_DIVIDER_RATIO 2  // 基板の分圧比 1/2 → ×2

// Core 1 が書き Core 0（BLE 通知）も読む。アトミックだが最適化で消えないよう volatile。
static volatile uint16_t cached_voltage_mv = 0;

// 単セル Li-Po の放電曲線（降順）。4.2V 満充電カーブを 4.05V=100% に比例スケールしたもの。
static const struct {
    uint16_t mv;
    uint8_t percent;
} battery_curve[] = {
    {4050, 100}, {3950, 89}, {3850, 72}, {3750, 56},
    {3650, 39},  {3550, 22}, {3450, 13}, {3350, 7}, {3250, 0},
};

void power_monitor_init(void) {
    // USB センスは外部分圧駆動、内部プルは電圧がずれるため無効。判定は usb_hid_is_connected() を使うため未読み出し。
    gpio_init(USB_SENSE_GPIO);
    gpio_set_dir(USB_SENSE_GPIO, GPIO_IN);
    gpio_disable_pulls(USB_SENSE_GPIO);

    adc_init();
    adc_gpio_init(BATTERY_ADC_GPIO);

    // 起動直後に Core 0 が BLE へ 0% を通知しないよう、ここで先に1回サンプリングする。
    power_monitor_update();
}

void power_monitor_update(void) {
    static absolute_time_t next_update = {0};
    if (!time_reached(next_update)) {
        return;
    }
    next_update = make_timeout_time_ms(POWER_SAMPLE_INTERVAL_MS);

    adc_select_input(BATTERY_ADC_CH);
    uint32_t sum = 0;
    for (uint8_t i = 0; i < POWER_ADC_OVERSAMPLE; ++i) {
        sum += adc_read();
    }
    const uint32_t raw = sum / POWER_ADC_OVERSAMPLE;
    cached_voltage_mv = (uint16_t)(raw * VREF_MV * BATTERY_DIVIDER_RATIO /
                                   (1u << ADC_BITS));
}

uint16_t power_monitor_get_battery_voltage_mv(void) {
    return cached_voltage_mv;
}

uint8_t power_monitor_get_battery_percent(void) {
    const uint16_t mv = cached_voltage_mv;
    const size_t points = sizeof(battery_curve) / sizeof(battery_curve[0]);

    if (mv >= battery_curve[0].mv) {
        return 100;
    }
    for (size_t i = 1; i < points; ++i) {
        if (mv < battery_curve[i].mv) {
            continue;
        }
        const uint16_t span_mv = battery_curve[i - 1].mv - battery_curve[i].mv;
        const uint8_t span_pct =
            battery_curve[i - 1].percent - battery_curve[i].percent;
        return (uint8_t)(battery_curve[i].percent +
                         (uint32_t)(mv - battery_curve[i].mv) * span_pct /
                             span_mv);
    }
    return 0;
}
