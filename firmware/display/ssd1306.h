#ifndef SSD1306_H
#define SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {

#endif

#define SSD1306_I2C_ADDR 0x3C  // 0x3C or 0x3D
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 32

extern uint8_t ssd1306_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

void ssd1306_init(void);
void ssd1306_clear_buffer(void);

// バッファの内容をパネルへ転送する
void ssd1306_display(void);

void ssd1306_set_pixel(uint8_t x, uint8_t y, bool on);
bool ssd1306_get_pixel(uint8_t x, uint8_t y);
void ssd1306_draw_hline(uint8_t x0, uint8_t y, uint8_t width, bool on);
void ssd1306_draw_vline(uint8_t x, uint8_t y0, uint8_t height, bool on);
void ssd1306_invert(bool invert);
void ssd1306_set_contrast(uint8_t contrast);
void ssd1306_power_on(bool on);

// 範囲外（FONT_FIRST_CHAR..FONT_LAST_CHAR 以外）の文字は描かれない
void ssd1306_draw_char(uint8_t x, uint8_t y, char ch, bool on);
void ssd1306_draw_string(uint8_t x, uint8_t y, const char* str, bool on);

// ASCII 印字可能文字（0x20-0x7E）に続けて 0x7F 以降へ連続配置している。
#define GLYPH_MARK '\x7F'
#define GLYPH_CROSS '\x80'
#define GLYPH_BATT_0 '\x81'
#define GLYPH_BATT_2 '\x82'
#define GLYPH_BATT_4 '\x83'
#define GLYPH_BATT_5 '\x84'
#define GLYPH_ARROW '\x85'  // 充電中
#define GLYPH_USB '\x86'    // USB（トライデント）

#define FONT_WIDTH 6
#define FONT_HEIGHT 8
#define FONT_FIRST_CHAR 0x20  // ' '
#define FONT_LAST_CHAR 0x86   // GLYPH_USB（末尾グリフ）
#define FONT_CHAR_COUNT (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)

extern const uint8_t font_data[FONT_CHAR_COUNT][FONT_WIDTH];

#ifdef __cplusplus
}
#endif

#endif  // SSD1306_H
