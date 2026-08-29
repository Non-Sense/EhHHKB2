"""SSD1306 128x32 / 6x8 フォントの表示内容を、ehhhkb2 ファームウェアのロジックに
忠実に再現して PNG 化するスクリプト。

display/ssd1306.c の font_data[]、ui/screen.c の draw_frame() / draw_config_menu() を
そのまま Python へ移植し、実機と同じピクセル配置で各画面を描画する。

使い方（初回のみ venv を作る）:
    python -m venv scripts/.venv
    scripts/.venv/Scripts/python.exe -m pip install pillow
    scripts/.venv/Scripts/python.exe scripts/render_screens.py
"""

import os
from PIL import Image, ImageDraw, ImageFont

# ---------------------------------------------------------------------------
# display/ssd1306.c の font_data[] をそのまま移植（6x8 フォント = 5x7 + 字間1px）
# ---------------------------------------------------------------------------
FONT_WIDTH = 6
FONT_HEIGHT = 8
FONT_FIRST_CHAR = 0x20
FONT_LAST_CHAR = 0x86

FONT_DATA = [
    [0x00, 0x00, 0x00, 0x00, 0x00, 0x00],  # ' ' (0x20)
    [0x00, 0x00, 0x5F, 0x00, 0x00, 0x00],  # '!' (0x21)
    [0x00, 0x07, 0x00, 0x07, 0x00, 0x00],  # '"' (0x22)
    [0x14, 0x7F, 0x14, 0x7F, 0x14, 0x00],  # '#' (0x23)
    [0x24, 0x2A, 0x7F, 0x2A, 0x12, 0x00],  # '$' (0x24)
    [0x23, 0x13, 0x08, 0x64, 0x62, 0x00],  # '%' (0x25)
    [0x36, 0x49, 0x55, 0x22, 0x50, 0x00],  # '&' (0x26)
    [0x00, 0x05, 0x03, 0x00, 0x00, 0x00],  # ''' (0x27)
    [0x00, 0x1C, 0x22, 0x41, 0x00, 0x00],  # '(' (0x28)
    [0x00, 0x41, 0x22, 0x1C, 0x00, 0x00],  # ')' (0x29)
    [0x14, 0x08, 0x3E, 0x08, 0x14, 0x00],  # '*' (0x2A)
    [0x08, 0x08, 0x3E, 0x08, 0x08, 0x00],  # '+' (0x2B)
    [0x00, 0x50, 0x30, 0x00, 0x00, 0x00],  # ',' (0x2C)
    [0x08, 0x08, 0x08, 0x08, 0x08, 0x00],  # '-' (0x2D)
    [0x00, 0x60, 0x60, 0x00, 0x00, 0x00],  # '.' (0x2E)
    [0x20, 0x10, 0x08, 0x04, 0x02, 0x00],  # '/' (0x2F)
    [0x3E, 0x51, 0x49, 0x45, 0x3E, 0x00],  # '0' (0x30)
    [0x00, 0x42, 0x7F, 0x40, 0x00, 0x00],  # '1' (0x31)
    [0x42, 0x61, 0x51, 0x49, 0x46, 0x00],  # '2' (0x32)
    [0x21, 0x41, 0x45, 0x4B, 0x31, 0x00],  # '3' (0x33)
    [0x18, 0x14, 0x12, 0x7F, 0x10, 0x00],  # '4' (0x34)
    [0x27, 0x45, 0x45, 0x45, 0x39, 0x00],  # '5' (0x35)
    [0x3C, 0x4A, 0x49, 0x49, 0x30, 0x00],  # '6' (0x36)
    [0x01, 0x71, 0x09, 0x05, 0x03, 0x00],  # '7' (0x37)
    [0x36, 0x49, 0x49, 0x49, 0x36, 0x00],  # '8' (0x38)
    [0x06, 0x49, 0x49, 0x29, 0x1E, 0x00],  # '9' (0x39)
    [0x00, 0x36, 0x36, 0x00, 0x00, 0x00],  # ':' (0x3A)
    [0x00, 0x56, 0x36, 0x00, 0x00, 0x00],  # ';' (0x3B)
    [0x08, 0x14, 0x22, 0x41, 0x00, 0x00],  # '<' (0x3C)
    [0x14, 0x14, 0x14, 0x14, 0x14, 0x00],  # '=' (0x3D)
    [0x00, 0x41, 0x22, 0x14, 0x08, 0x00],  # '>' (0x3E)
    [0x02, 0x01, 0x51, 0x09, 0x06, 0x00],  # '?' (0x3F)
    [0x32, 0x49, 0x79, 0x41, 0x3E, 0x00],  # '@' (0x40)
    [0x7E, 0x11, 0x11, 0x11, 0x7E, 0x00],  # 'A' (0x41)
    [0x7F, 0x49, 0x49, 0x49, 0x36, 0x00],  # 'B' (0x42)
    [0x3E, 0x41, 0x41, 0x41, 0x22, 0x00],  # 'C' (0x43)
    [0x7F, 0x41, 0x41, 0x22, 0x1C, 0x00],  # 'D' (0x44)
    [0x7F, 0x49, 0x49, 0x49, 0x41, 0x00],  # 'E' (0x45)
    [0x7F, 0x09, 0x09, 0x09, 0x01, 0x00],  # 'F' (0x46)
    [0x3E, 0x41, 0x49, 0x49, 0x7A, 0x00],  # 'G' (0x47)
    [0x7F, 0x08, 0x08, 0x08, 0x7F, 0x00],  # 'H' (0x48)
    [0x00, 0x41, 0x7F, 0x41, 0x00, 0x00],  # 'I' (0x49)
    [0x20, 0x40, 0x41, 0x3F, 0x01, 0x00],  # 'J' (0x4A)
    [0x7F, 0x08, 0x14, 0x22, 0x41, 0x00],  # 'K' (0x4B)
    [0x7F, 0x40, 0x40, 0x40, 0x40, 0x00],  # 'L' (0x4C)
    [0x7F, 0x02, 0x0C, 0x02, 0x7F, 0x00],  # 'M' (0x4D)
    [0x7F, 0x04, 0x08, 0x10, 0x7F, 0x00],  # 'N' (0x4E)
    [0x3E, 0x41, 0x41, 0x41, 0x3E, 0x00],  # 'O' (0x4F)
    [0x7F, 0x09, 0x09, 0x09, 0x06, 0x00],  # 'P' (0x50)
    [0x3E, 0x41, 0x51, 0x21, 0x5E, 0x00],  # 'Q' (0x51)
    [0x7F, 0x09, 0x19, 0x29, 0x46, 0x00],  # 'R' (0x52)
    [0x46, 0x49, 0x49, 0x49, 0x31, 0x00],  # 'S' (0x53)
    [0x01, 0x01, 0x7F, 0x01, 0x01, 0x00],  # 'T' (0x54)
    [0x3F, 0x40, 0x40, 0x40, 0x3F, 0x00],  # 'U' (0x55)
    [0x1F, 0x20, 0x40, 0x20, 0x1F, 0x00],  # 'V' (0x56)
    [0x3F, 0x40, 0x38, 0x40, 0x3F, 0x00],  # 'W' (0x57)
    [0x63, 0x14, 0x08, 0x14, 0x63, 0x00],  # 'X' (0x58)
    [0x03, 0x04, 0x78, 0x04, 0x03, 0x00],  # 'Y' (0x59)
    [0x61, 0x51, 0x49, 0x45, 0x43, 0x00],  # 'Z' (0x5A)
    [0x00, 0x7F, 0x41, 0x41, 0x00, 0x00],  # '[' (0x5B)
    [0x02, 0x04, 0x08, 0x10, 0x20, 0x00],  # '\\' (0x5C)
    [0x00, 0x41, 0x41, 0x7F, 0x00, 0x00],  # ']' (0x5D)
    [0x04, 0x02, 0x01, 0x02, 0x04, 0x00],  # '^' (0x5E)
    [0x40, 0x40, 0x40, 0x40, 0x40, 0x00],  # '_' (0x5F)
    [0x00, 0x01, 0x02, 0x04, 0x00, 0x00],  # '`' (0x60)
    [0x20, 0x54, 0x54, 0x54, 0x78, 0x00],  # 'a' (0x61)
    [0x7F, 0x48, 0x44, 0x44, 0x38, 0x00],  # 'b' (0x62)
    [0x38, 0x44, 0x44, 0x44, 0x20, 0x00],  # 'c' (0x63)
    [0x38, 0x44, 0x44, 0x48, 0x7F, 0x00],  # 'd' (0x64)
    [0x38, 0x54, 0x54, 0x54, 0x18, 0x00],  # 'e' (0x65)
    [0x08, 0x7E, 0x09, 0x01, 0x02, 0x00],  # 'f' (0x66)
    [0x08, 0x14, 0x54, 0x54, 0x3C, 0x00],  # 'g' (0x67)
    [0x7F, 0x08, 0x04, 0x04, 0x78, 0x00],  # 'h' (0x68)
    [0x00, 0x44, 0x7D, 0x40, 0x00, 0x00],  # 'i' (0x69)
    [0x20, 0x40, 0x44, 0x3D, 0x00, 0x00],  # 'j' (0x6A)
    [0x7F, 0x10, 0x28, 0x44, 0x00, 0x00],  # 'k' (0x6B)
    [0x00, 0x41, 0x7F, 0x40, 0x00, 0x00],  # 'l' (0x6C)
    [0x7C, 0x04, 0x18, 0x04, 0x78, 0x00],  # 'm' (0x6D)
    [0x7C, 0x08, 0x04, 0x04, 0x78, 0x00],  # 'n' (0x6E)
    [0x38, 0x44, 0x44, 0x44, 0x38, 0x00],  # 'o' (0x6F)
    [0x7C, 0x14, 0x14, 0x14, 0x08, 0x00],  # 'p' (0x70)
    [0x08, 0x14, 0x14, 0x18, 0x7C, 0x00],  # 'q' (0x71)
    [0x7C, 0x08, 0x04, 0x04, 0x08, 0x00],  # 'r' (0x72)
    [0x48, 0x54, 0x54, 0x54, 0x20, 0x00],  # 's' (0x73)
    [0x04, 0x3F, 0x44, 0x40, 0x20, 0x00],  # 't' (0x74)
    [0x3C, 0x40, 0x40, 0x20, 0x7C, 0x00],  # 'u' (0x75)
    [0x1C, 0x20, 0x40, 0x20, 0x1C, 0x00],  # 'v' (0x76)
    [0x3C, 0x40, 0x30, 0x40, 0x3C, 0x00],  # 'w' (0x77)
    [0x44, 0x28, 0x10, 0x28, 0x44, 0x00],  # 'x' (0x78)
    [0x0C, 0x50, 0x50, 0x50, 0x3C, 0x00],  # 'y' (0x79)
    [0x44, 0x64, 0x54, 0x4C, 0x44, 0x00],  # 'z' (0x7A)
    [0x00, 0x08, 0x36, 0x41, 0x00, 0x00],  # '{' (0x7B)
    [0x00, 0x00, 0x7F, 0x00, 0x00, 0x00],  # '|' (0x7C)
    [0x00, 0x41, 0x36, 0x08, 0x00, 0x00],  # '}' (0x7D)
    [0x08, 0x04, 0x08, 0x10, 0x08, 0x00],  # '~' (0x7E)
    [0x22, 0x14, 0x7F, 0x2A, 0x14, 0x00],  # 0x7F: BTマーク
    [0x22, 0x14, 0x08, 0x14, 0x22, 0x00],  # 0x80: 取り消し線ではない、✕印
    [0x7E, 0x41, 0x41, 0x7E, 0x00, 0x00],  # 0x81: バッテリー残量 0/5
    [0x7E, 0x71, 0x71, 0x7E, 0x00, 0x00],  # 0x82: バッテリー残量 2/5
    [0x7E, 0x7D, 0x7D, 0x7E, 0x00, 0x00],  # 0x83: バッテリー残量 4/5
    [0x7E, 0x7F, 0x7F, 0x7E, 0x00, 0x00],  # 0x84: バッテリー残量 5/5
    [0x08, 0x0C, 0x3E, 0x18, 0x08, 0x00],  # 0x85: 充電中
    [0x0C, 0x62, 0x7F, 0x62, 0x0C, 0x00],  # 0x86: USB（トライデント）
]

GLYPH_MARK = '\x7F'
GLYPH_CROSS = '\x80'
GLYPH_BATT_0 = '\x81'
GLYPH_BATT_2 = '\x82'
GLYPH_BATT_4 = '\x83'
GLYPH_BATT_5 = '\x84'
GLYPH_ARROW = '\x85'
GLYPH_USB = '\x86'

SSD1306_WIDTH = 128
SSD1306_HEIGHT = 32

HID_LED_NUM_LOCK = 1 << 0
HID_LED_CAPS_LOCK = 1 << 1
HID_LED_SCROLL_LOCK = 1 << 2

# ---------------------------------------------------------------------------
# ui/config_menu.h の定数・列挙をそのまま移植
# ---------------------------------------------------------------------------
BLE_HOST_SLOTS = 6
BLE_HOST_NAME_MAX = 10

(CFG_OFF, CFG_TOP, CFG_CONNECT, CFG_RESET, CFG_RENAME, CFG_EDIT, CFG_MACMODE,
 CFG_SWAP, CFG_SWAP_TARGET, CFG_MISC, CFG_BATTERY, CFG_BOOT) = range(12)

(CFG_TOP_CONNECT, CFG_TOP_PAIR, CFG_TOP_MACMODE, CFG_TOP_RENAME, CFG_TOP_SWAP,
 CFG_TOP_RESET, CFG_TOP_MISC) = range(7)
CFG_TOP_COUNT = 7

# CFG_MISC の項目（Boot mode は破壊的操作なので末尾）
CFG_MISC_BATTERY, CFG_MISC_DISPLAY_OFF, CFG_MISC_QUIET_MODE, CFG_MISC_BOOT = range(4)
CFG_MISC_COUNT = 4

CFG_VISIBLE_ROWS = 4
CFG_LABEL_MAX = 16
CFG_ITEM_MAX = max(BLE_HOST_SLOTS + 1, CFG_TOP_COUNT, CFG_MISC_COUNT)


def cfg_item_count(screen):
    if screen == CFG_TOP:
        return CFG_TOP_COUNT
    if screen == CFG_CONNECT:
        return BLE_HOST_SLOTS + 1
    if screen in (CFG_RESET, CFG_RENAME, CFG_MACMODE, CFG_SWAP, CFG_SWAP_TARGET):
        return BLE_HOST_SLOTS
    if screen == CFG_MISC:
        return CFG_MISC_COUNT
    if screen in (CFG_BATTERY, CFG_BOOT):
        return 1
    return 0


# ---------------------------------------------------------------------------
# display/ssd1306.c の描画関数群をそのまま移植した簡易フレームバッファ
# ---------------------------------------------------------------------------
class SSD1306:
    def __init__(self):
        self.px = [[False] * SSD1306_WIDTH for _ in range(SSD1306_HEIGHT)]

    def set_pixel(self, x, y, on):
        if x >= SSD1306_WIDTH or y >= SSD1306_HEIGHT or x < 0 or y < 0:
            return
        self.px[y][x] = on

    def draw_hline(self, x0, y, width, on):
        for x in range(x0, x0 + width):
            if x >= SSD1306_WIDTH:
                break
            self.set_pixel(x, y, on)

    def draw_vline(self, x, y0, height, on):
        for y in range(y0, y0 + height):
            if y >= SSD1306_HEIGHT:
                break
            self.set_pixel(x, y, on)

    def draw_char(self, x, y, ch, on):
        uch = ord(ch)
        if uch < FONT_FIRST_CHAR or uch > FONT_LAST_CHAR:
            return
        data = FONT_DATA[uch - FONT_FIRST_CHAR]
        for col in range(FONT_WIDTH):
            pixel_data = data[col]
            for bit in range(8):
                if x + col >= SSD1306_WIDTH:
                    return
                pixel_on = (pixel_data & (1 << bit)) != 0
                self.set_pixel(x + col, y + bit, on and pixel_on)

    def draw_string(self, x, y, s, on):
        for ch in s:
            self.draw_char(x, y, ch, on)
            x += FONT_WIDTH

    def to_image(self, scale=10, fg=(224, 245, 255), bg=(4, 8, 14)):
        img = Image.new("RGB", (SSD1306_WIDTH * scale, SSD1306_HEIGHT * scale), bg)
        pixels = img.load()
        for y in range(SSD1306_HEIGHT):
            for x in range(SSD1306_WIDTH):
                if self.px[y][x]:
                    for dy in range(scale):
                        for dx in range(scale):
                            pixels[x * scale + dx, y * scale + dy] = fg
        return img


# ---------------------------------------------------------------------------
# ui/screen.c の draw_frame() 系をそのまま移植
# ---------------------------------------------------------------------------
class HostState:
    def __init__(self):
        self.connected = False
        self.active_slot = 0
        self.slot_has_bond = [False] * BLE_HOST_SLOTS
        self.slot_name = [""] * BLE_HOST_SLOTS
        self.slot_mac_mode = [False] * BLE_HOST_SLOTS


class UiState:
    def __init__(self):
        self.host = HostState()
        self.pairing = False
        self.display_forced_off = False
        self.quiet_mode_on = False
        self.bt_disabled = False
        self.usb_connected = False
        self.has_code = False
        self.code = 0
        self.display_on = True
        self.leds = 0
        self.booting = False
        self.cfg_screen = CFG_OFF
        self.cfg_cursor = 0
        self.cfg_rename_slot = 0
        self.cfg_swap_from_slot = 0
        self.cfg_edit = ""
        # 説明用のダミー電圧。% は compute_battery_percent() で都度算出する。
        self.battery_mv = 3900


def draw_led_indicators(d: SSD1306, leds):
    STATUS_LINE_Y = 24
    LED_X_NUM = 0
    LED_X_CAPS = 4 * FONT_WIDTH
    LED_X_SCROLL = 8 * FONT_WIDTH
    if leds & HID_LED_NUM_LOCK:
        d.draw_string(LED_X_NUM, STATUS_LINE_Y, "NUM", True)
    if leds & HID_LED_CAPS_LOCK:
        d.draw_string(LED_X_CAPS, STATUS_LINE_Y, "CAP", True)
    if leds & HID_LED_SCROLL_LOCK:
        d.draw_string(LED_X_SCROLL, STATUS_LINE_Y, "SCR", True)


# power/power_monitor.c の battery_curve[] / power_monitor_get_battery_percent()
# をそのまま移植。Battery 画面・BLE 通知・残量アイコンは全てこの一箇所から
# 計算した % を参照するため、表示は常に一致する。
BATTERY_CURVE = [
    (4050, 100), (3950, 89), (3850, 72), (3750, 56),
    (3650, 39), (3550, 22), (3450, 13), (3350, 7), (3250, 0),
]


def compute_battery_percent(mv):
    if mv >= BATTERY_CURVE[0][0]:
        return 100
    for i in range(1, len(BATTERY_CURVE)):
        hi_mv, hi_pct = BATTERY_CURVE[i - 1]
        lo_mv, lo_pct = BATTERY_CURVE[i]
        if mv < lo_mv:
            continue
        span_mv = hi_mv - lo_mv
        span_pct = hi_pct - lo_pct
        return lo_pct + (mv - lo_mv) * span_pct // span_mv
    return 0


def draw_booting_screen(d: SSD1306):
    msg = "Booting..."
    x = (SSD1306_WIDTH - len(msg) * FONT_WIDTH) // 2
    y = (SSD1306_HEIGHT - FONT_HEIGHT) // 2
    d.draw_string(x, y, msg, True)


def draw_battery_screen(d: SSD1306, s: UiState):
    mv = s.battery_mv
    pct = compute_battery_percent(mv)
    d.draw_string(0, 0, "Battery", True)
    buf = "%u.%02u V  %u%%" % (mv // 1000, (mv % 1000) // 10, pct)
    d.draw_string(0, 12, buf, True)


def slot_display_number(slot):
    """内部スロット番号（0始まり）を表示用の BT 番号へ変換する。
    基板の配線都合で内部順序と実物のラベルが逆になっているため、表示側だけ反転させる
    （ui/screen.c の slot_display_number() と同じ）。"""
    return BLE_HOST_SLOTS - slot


def cfg_slot_for_row(row):
    """一覧画面（Connect/BLE reset/Rename）のカーソル位置（表示行、0始まり=最上段）
    を実際の BLE ホストスロット番号へ変換する。一覧は表示上 BT1→BT6 の昇順に
    並ぶように、この変換を通してスロットを参照する
    （ui/config_menu.h の cfg_slot_for_row() と同じ。自己逆変換）。"""
    return BLE_HOST_SLOTS - 1 - row


def draw_connection_status(d: SSD1306, s: UiState):
    slot = s.host.active_slot
    if s.bt_disabled:
        buf = "USB"
    elif s.host.connected and slot < BLE_HOST_SLOTS:
        buf = s.host.slot_name[slot] if s.host.slot_name[slot] else "BT%u" % slot_display_number(slot)
    elif s.pairing:
        buf = "PAIR"
    else:
        buf = "---"
    d.draw_string(0, 0, buf, True)


# ui/screen.c の BATTERY_PCT_FULL / HIGH / LOW と同じ
BATTERY_PCT_FULL = 75
BATTERY_PCT_HIGH = 50
BATTERY_PCT_LOW = 25


def draw_battery_icon(d: SSD1306, s: UiState):
    bx = SSD1306_WIDTH - FONT_WIDTH
    if s.usb_connected:
        d.draw_char(bx, 0, GLYPH_ARROW, True)
        d.draw_char(bx - FONT_WIDTH, 0, GLYPH_USB, True)
    else:
        pct = compute_battery_percent(s.battery_mv)
        if pct >= BATTERY_PCT_FULL:
            glyph = GLYPH_BATT_5
        elif pct >= BATTERY_PCT_HIGH:
            glyph = GLYPH_BATT_4
        elif pct >= BATTERY_PCT_LOW:
            glyph = GLYPH_BATT_2
        else:
            glyph = GLYPH_BATT_0
        d.draw_char(bx, 0, glyph, True)


def draw_mac_mode_indicator(d: SSD1306, s: UiState):
    """接続中スロットが Mac モード ON のときだけ、電池アイコンの左に表示する
    （ui/screen.c の draw_mac_mode_indicator() と同じ）。"""
    slot = s.host.active_slot
    if s.bt_disabled or not s.host.connected or slot >= BLE_HOST_SLOTS:
        return
    if not s.host.slot_mac_mode[slot]:
        return
    d.draw_char(SSD1306_WIDTH - 3 * FONT_WIDTH, 0, 'M', True)


def slot_label(slot, names):
    return names[slot] if names[slot] else "BT%d" % slot_display_number(slot)


def draw_config_menu(d: SSD1306, s: UiState):
    screen = s.cfg_screen
    cursor = s.cfg_cursor
    names = s.host.slot_name

    if screen == CFG_BATTERY:
        draw_battery_screen(d, s)
        return

    if screen == CFG_EDIT:
        hdr = "Rename BT%d" % slot_display_number(s.cfg_rename_slot)
        d.draw_string(0, 0, hdr, True)
        line = "%s_" % s.cfg_edit
        d.draw_string(0, 12, line, True)
        d.draw_string(0, 24, "Ent:OK Esc:X", True)
        return

    texts = [""] * CFG_ITEM_MAX
    struck = [False] * CFG_ITEM_MAX
    conn = [False] * CFG_ITEM_MAX
    mac_on = [False] * CFG_ITEM_MAX
    from_mark = [False] * CFG_ITEM_MAX
    count = cfg_item_count(screen)

    if screen == CFG_TOP:
        texts[CFG_TOP_CONNECT] = "Connect"
        texts[CFG_TOP_PAIR] = "CancelPair" if s.pairing else "Pairing"
        texts[CFG_TOP_MACMODE] = "Mac mode"
        texts[CFG_TOP_RENAME] = "BLE rename"
        texts[CFG_TOP_SWAP] = "BLE swap"
        texts[CFG_TOP_RESET] = "BLE reset"
        texts[CFG_TOP_MISC] = "Misc"
    elif screen == CFG_MISC:
        texts[CFG_MISC_BATTERY] = "Battery"
        texts[CFG_MISC_DISPLAY_OFF] = "Display On" if s.display_forced_off else "Display Off"
        texts[CFG_MISC_QUIET_MODE] = "Quiet Off" if s.quiet_mode_on else "Quiet On"
        texts[CFG_MISC_BOOT] = "Boot mode"
    elif screen == CFG_CONNECT:
        texts[0] = "USB"
        conn[0] = s.bt_disabled
        for i in range(BLE_HOST_SLOTS):
            slot = cfg_slot_for_row(i)
            texts[i + 1] = slot_label(slot, names)
            conn[i + 1] = s.host.connected and s.host.active_slot == slot
            struck[i + 1] = not s.host.slot_has_bond[slot]
    elif screen == CFG_RESET:
        for i in range(BLE_HOST_SLOTS):
            slot = cfg_slot_for_row(i)
            texts[i] = slot_label(slot, names)
            struck[i] = not s.host.slot_has_bond[slot]
            conn[i] = s.host.connected and s.host.active_slot == slot
    elif screen == CFG_RENAME:
        for i in range(BLE_HOST_SLOTS):
            slot = cfg_slot_for_row(i)
            if names[slot]:
                texts[i] = "BT%d %s" % (slot_display_number(slot), names[slot])
            else:
                texts[i] = "BT%d" % slot_display_number(slot)
    elif screen == CFG_MACMODE:
        for i in range(BLE_HOST_SLOTS):
            slot = cfg_slot_for_row(i)
            texts[i] = slot_label(slot, names)
            mac_on[i] = s.host.slot_mac_mode[slot]
    elif screen == CFG_SWAP:
        for i in range(BLE_HOST_SLOTS):
            slot = cfg_slot_for_row(i)
            texts[i] = slot_label(slot, names)
            struck[i] = not s.host.slot_has_bond[slot]
            conn[i] = s.host.connected and s.host.active_slot == slot
    elif screen == CFG_SWAP_TARGET:
        for i in range(BLE_HOST_SLOTS):
            slot = cfg_slot_for_row(i)
            texts[i] = slot_label(slot, names)
            struck[i] = not s.host.slot_has_bond[slot]
            conn[i] = s.host.connected and s.host.active_slot == slot
            from_mark[i] = slot == s.cfg_swap_from_slot
    else:  # CFG_BOOT
        texts[0] = "continue?"

    first = 0
    if count > CFG_VISIBLE_ROWS:
        first = cursor - 1
        if first < 0:
            first = 0
        if first > count - CFG_VISIBLE_ROWS:
            first = count - CFG_VISIBLE_ROWS

    row = 0
    while row < CFG_VISIBLE_ROWS and (first + row) < count:
        idx = first + row
        line = "%c %s" % ('>' if idx == cursor else ' ', texts[idx])
        y = row * 8
        d.draw_string(0, y, line, True)
        if struck[idx]:
            length = len(line)
            d.draw_hline(2 * FONT_WIDTH, y + 3, (length - 2) * FONT_WIDTH, True)
        if screen == CFG_MACMODE:
            cx = SSD1306_WIDTH - 3 * FONT_WIDTH
            d.draw_string(cx, y, " ON" if mac_on[idx] else "OFF", True)
        elif screen == CFG_SWAP_TARGET and from_mark[idx]:
            cx = SSD1306_WIDTH - 3 * FONT_WIDTH
            d.draw_string(cx, y, "(F)", True)
        elif conn[idx]:
            cx = SSD1306_WIDTH - 3 * FONT_WIDTH
            d.draw_string(cx, y, "(C)", True)
        row += 1


def draw_frame(s: UiState) -> SSD1306:
    d = SSD1306()
    if s.booting:
        draw_booting_screen(d)
    elif s.cfg_screen != CFG_OFF:
        draw_config_menu(d, s)
    else:
        draw_connection_status(d, s)
        draw_mac_mode_indicator(d, s)
        draw_battery_icon(d, s)
        if s.has_code:
            code_str = "Code:%06lu" % s.code
            d.draw_string(0, 12, code_str, True)
        draw_led_indicators(d, s.leds)
    return d


# ---------------------------------------------------------------------------
# 各画面のシナリオ定義
# ---------------------------------------------------------------------------
# 一覧画面のデモ用ホスト状態。内部スロット番号は表示番号の逆順（内部 5 = BT1）なので、
# 一覧の上から順に埋まって見えるよう、名前とボンドは大きいスロット番号側へ置く。
DEMO_SLOTS = [
    # (内部スロット, 名前, ボンドあり, Mac モード)  … 表示は上から BT1, BT2, ...
    (5, "MacBookPro", True, True),
    (4, "Win Note", True, False),
    (3, "", True, False),
    (2, "", False, False),
    (1, "", False, False),
    (0, "", False, False),
]


def demo_menu_state(screen, cursor=0, **kwargs):
    """一覧画面用の UiState を作る。BT1 に接続中・BT1〜BT3 がペアリング済み。"""
    s = UiState()
    for slot, name, bond, mac in DEMO_SLOTS:
        s.host.slot_name[slot] = name
        s.host.slot_has_bond[slot] = bond
        s.host.slot_mac_mode[slot] = mac
    s.host.connected = True
    s.host.active_slot = 5  # 表示上の BT1
    s.cfg_screen = screen
    s.cfg_cursor = cursor
    for key, value in kwargs.items():
        assert hasattr(s, key), key
        setattr(s, key, value)
    return s


def make_scenarios():
    scenarios = {}

    # --- 通常表示（CFG_OFF） ---
    s = UiState()
    s.host.connected = True
    s.host.active_slot = 5
    s.host.slot_name[5] = "MacBookPro"
    s.battery_mv = 4050
    scenarios["01_status_bt_named"] = s

    s = UiState()
    s.host.connected = True
    s.host.active_slot = 4
    s.battery_mv = 3650
    scenarios["02_status_bt_unnamed"] = s

    s = UiState()
    s.bt_disabled = True
    s.usb_connected = True
    scenarios["03_status_usb"] = s

    # ble_hid.c は IO_CAPABILITY_NO_INPUT_NO_OUTPUT（Just Works）なので
    # SM_EVENT_PASSKEY_DISPLAY_NUMBER は発行されず、has_code は立たない。
    # draw_frame() の Code 行は現状の設定では到達しないため、ここでも出さない。
    s = UiState()
    s.pairing = True
    s.battery_mv = 3900
    scenarios["04_status_pairing"] = s

    s = UiState()
    s.battery_mv = 3400
    scenarios["05_status_disconnected"] = s

    s = UiState()
    s.host.connected = True
    s.host.active_slot = 5
    s.host.slot_name[5] = "MacBookPro"
    s.host.slot_mac_mode[5] = True
    s.battery_mv = 4050
    scenarios["06_status_mac_mode"] = s

    s = UiState()
    s.host.connected = True
    s.host.active_slot = 5
    s.host.slot_name[5] = "MacBookPro"
    s.battery_mv = 3750
    s.leds = HID_LED_NUM_LOCK | HID_LED_CAPS_LOCK | HID_LED_SCROLL_LOCK
    scenarios["07_status_leds"] = s

    # --- コンフィグメニュー ---
    # トップは 7 項目あり 4 行しか入らないので、先頭とスクロール後の両方を出す。
    scenarios["10_menu_top"] = demo_menu_state(CFG_TOP, CFG_TOP_CONNECT)
    scenarios["11_menu_top_scrolled"] = demo_menu_state(CFG_TOP, CFG_TOP_MISC)
    scenarios["12_menu_connect"] = demo_menu_state(CFG_CONNECT, 1)
    scenarios["13_menu_macmode"] = demo_menu_state(CFG_MACMODE, 0)
    scenarios["14_menu_rename"] = demo_menu_state(CFG_RENAME, 0)
    scenarios["15_menu_edit"] = demo_menu_state(
        CFG_EDIT, 0, cfg_rename_slot=5, cfg_edit="MacBook")
    scenarios["16_menu_swap"] = demo_menu_state(CFG_SWAP, 0)
    scenarios["17_menu_swap_target"] = demo_menu_state(
        CFG_SWAP_TARGET, 1, cfg_swap_from_slot=5)
    scenarios["18_menu_reset"] = demo_menu_state(CFG_RESET, 1)
    scenarios["19_menu_misc"] = demo_menu_state(CFG_MISC, 0)
    scenarios["20_menu_battery"] = demo_menu_state(
        CFG_BATTERY, 0, battery_mv=3870)
    scenarios["21_menu_boot"] = demo_menu_state(CFG_BOOT, 0)

    s = UiState()
    s.booting = True
    scenarios["22_booting"] = s

    return scenarios


def render_glyph_icons(out_dir, scale=16):
    """凡例用に個別グリフだけを FONT_WIDTH x FONT_HEIGHT で切り出す。"""
    icons = {
        "icon_batt_0": GLYPH_BATT_0,
        "icon_batt_2": GLYPH_BATT_2,
        "icon_batt_4": GLYPH_BATT_4,
        "icon_batt_5": GLYPH_BATT_5,
        "icon_charge": GLYPH_ARROW,
        "icon_usb": GLYPH_USB,
        "icon_bt_mark": GLYPH_MARK,
        "icon_cross": GLYPH_CROSS,
    }
    for name, glyph in icons.items():
        d = SSD1306()
        d.draw_char(0, 0, glyph, True)
        # FONT_WIDTH x FONT_HEIGHT の領域だけ切り出して拡大する
        img = Image.new("RGB", (FONT_WIDTH * scale, FONT_HEIGHT * scale), (4, 8, 14))
        pixels = img.load()
        for y in range(FONT_HEIGHT):
            for x in range(FONT_WIDTH):
                if d.px[y][x]:
                    for dy in range(scale):
                        for dx in range(scale):
                            pixels[x * scale + dx, y * scale + dy] = (224, 245, 255)
        path = os.path.join(out_dir, f"{name}.png")
        img.save(path)
        print("wrote", path)


# 0x7F 以降の独自グリフ名（display/ssd1306.h の GLYPH_* と対応）
FONT_SHEET_GLYPH_NAMES = {
    0x7F: "MARK",
    0x80: "CROSS",
    0x81: "BATT0",
    0x82: "BATT2",
    0x83: "BATT4",
    0x84: "BATT5",
    0x85: "CHRG",
    0x86: "USB",
}


def load_label_font(size):
    """ラベル用の等幅 TTF を探す。Windows / Linux のどちらでも見つかるよう候補を並べ、
    どれも無ければ Pillow 内蔵フォントへ落とす（内蔵の既定は極小なのでサイズを指定する）。"""
    for name in ("consola.ttf", "DejaVuSansMono.ttf", "cour.ttf", "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    try:
        return ImageFont.load_default(size=size)
    except TypeError:  # Pillow < 10.1
        return ImageFont.load_default()


# scale と gap は、書き出した画像を manual.html で等倍表示したときにラベルが
# 潰れない寸法にしてある（幅 766px ≒ 本文カラム幅）。変える場合は
# build_manual.py の .font-sheet img（width: auto）も合わせて見直すこと。
def render_font_sheet(out_dir, scale=6, cols=16):
    """font_data[] の全 103 文字（0x20-0x86）を並べた一覧画像を書き出す。"""
    count = len(FONT_DATA)
    rows = (count + cols - 1) // cols

    label_size = 13
    cell_w = FONT_WIDTH * scale
    label_h = 30
    cell_h = FONT_HEIGHT * scale + label_h
    gap = 10
    margin = 20

    img_w = margin * 2 + cols * cell_w + (cols - 1) * gap
    img_h = margin * 2 + rows * cell_h + (rows - 1) * gap

    bg = (4, 8, 14)
    fg = (224, 245, 255)
    label_fg = (168, 186, 200)

    img = Image.new("RGB", (img_w, img_h), bg)
    draw = ImageDraw.Draw(img)
    font = load_label_font(label_size)

    for i in range(count):
        ch_code = FONT_FIRST_CHAR + i
        col = i % cols
        row = i // cols
        x0 = margin + col * (cell_w + gap)
        y0 = margin + row * (cell_h + gap)

        data = FONT_DATA[i]
        for cx in range(FONT_WIDTH):
            colbits = data[cx]
            for cy in range(8):
                if colbits & (1 << cy):
                    px, py = x0 + cx * scale, y0 + cy * scale
                    draw.rectangle([px, py, px + scale - 1, py + scale - 1], fill=fg)

        if ch_code == 0x20:
            sub = "SPACE"
        elif ch_code in FONT_SHEET_GLYPH_NAMES:
            sub = FONT_SHEET_GLYPH_NAMES[ch_code]
        else:
            sub = chr(ch_code)

        ly = y0 + FONT_HEIGHT * scale + 4
        draw.text((x0, ly), "0x%02X" % ch_code, font=font, fill=label_fg)
        draw.text((x0, ly + label_size + 2), sub, font=font, fill=fg)

    path = os.path.join(out_dir, "font_sheet.png")
    img.save(path)
    print("wrote", path)


def main():
    out_dir = os.path.join(os.path.dirname(__file__), "out")
    os.makedirs(out_dir, exist_ok=True)
    scenarios = make_scenarios()
    for name, state in scenarios.items():
        d = draw_frame(state)
        img = d.to_image(scale=10)
        path = os.path.join(out_dir, f"{name}.png")
        img.save(path)
        print("wrote", path)
    render_glyph_icons(out_dir)
    render_font_sheet(out_dir)


if __name__ == "__main__":
    main()