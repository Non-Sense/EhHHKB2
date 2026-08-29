"""render_screens.py が書き出した PNG を base64 で埋め込み、
メニュー操作ガイド（1 枚の自己完結 HTML）を組み立てるスクリプト。

画面の並び・項目名は ui/config_menu.h と ui/screen.c に合わせてある。
ファームウェア側でメニュー構成を変えたら、render_screens.py と一緒にここも直す。

使い方（初回のみ venv を作る）:
    python -m venv scripts/.venv
    scripts/.venv/Scripts/python.exe -m pip install pillow
    scripts/.venv/Scripts/python.exe scripts/render_screens.py   # 先に画像生成
    scripts/.venv/Scripts/python.exe scripts/build_manual.py
"""

import base64
import os

HERE = os.path.dirname(__file__)
OUT_DIR = os.path.join(HERE, "out")


def b64(name):
    path = os.path.join(OUT_DIR, f"{name}.png")
    with open(path, "rb") as f:
        return base64.b64encode(f.read()).decode("ascii")


def img_tag(name, alt, cls="screen-img"):
    return f'<img class="{cls}" src="data:image/png;base64,{b64(name)}" alt="{alt}" width="128" height="32">'


def icon_tag(name, alt):
    return f'<img class="icon-img" src="data:image/png;base64,{b64(name)}" alt="{alt}" width="6" height="8">'


def screen_card(shot, title, kbd, desc_html):
    return f"""
      <article class="card">
        <div class="oled">{img_tag(shot, title)}</div>
        <div class="card-body">
          <div class="card-head">
            <h3>{title}</h3>
            <span class="kbd-hint">{kbd}</span>
          </div>
          {desc_html}
        </div>
      </article>"""


def icon_row(name, alt, label, desc):
    return f"""
        <div class="icon-row">
          <div class="icon-frame">{icon_tag(name, alt)}</div>
          <div class="icon-text">
            <span class="icon-label">{label}</span>
            <span class="icon-desc">{desc}</span>
          </div>
        </div>"""


# ---------------------------------------------------------------------------
# 画面遷移図。依存ライブラリ無しで描くインライン SVG。
# 列ごとに (左端 x, 幅) を持ち、行は FLOW_ROW0 + FLOW_PITCH * row が箱の中心 y。
# ---------------------------------------------------------------------------
# viewBox の幅は .flow-svg の min-width（820px）とほぼ同じにしてある。縮小表示に
# なると 10px 前後の補足テキストが潰れるため、等倍〜わずかな拡大で収まる寸法にする。
FLOW_COLS = [(8, 120), (184, 112), (336, 124), (500, 136), (676, 126)]
FLOW_ROW0 = 30
FLOW_PITCH = 52
FLOW_BOX_H = 40
FLOW_W = 810
FLOW_H = 530

# id: (列, 行, 見出し, 補足, 種別)
#   close = 決定するとメニューを閉じる / stay = 決定後も画面に留まる / term = 終端
FLOW_NODES = {
    "status":  (0, 3, "ステータス画面", "通常表示", "plain"),
    "top":     (1, 3, "Top menu", "7 項目", "plain"),
    "connect": (2, 0, "Connect", "USB / BT1-6", "close"),
    "pairing": (2, 1, "Pairing", "開始 / 中止", "close"),
    "macmode": (2, 2, "Mac mode", "ON/OFF 切替", "stay"),
    "rename":  (2, 3, "BLE rename", "スロット選択", "plain"),
    "swap":    (2, 4, "BLE swap", "入れ替え元", "plain"),
    "reset":   (2, 5, "BLE reset", "ボンド削除", "stay"),
    "misc":    (2, 6, "Misc", "4 項目", "plain"),
    "edit":    (3, 3, "名前入力", "Ent:OK Esc:X", "plain"),
    "swap2":   (3, 4, "入れ替え先", "元は (F)", "stay"),
    "battery": (3, 6, "Battery", "電圧 + 残量", "plain"),
    "disp":    (3, 7, "Display On/Off", "手動消灯の切替", "close"),
    "quiet":   (3, 8, "Quiet On/Off", "SMPS モード切替", "close"),
    "boot":    (3, 9, "Boot mode", "continue?", "plain"),
    "bootsel": (4, 9, "BOOTSEL 再起動", "UF2 書き込み", "term"),
}

FLOW_EDGES = [
    ("status", "top", "FN2+End"),
    ("top", "connect", ""),
    ("top", "pairing", ""),
    ("top", "macmode", ""),
    ("top", "rename", ""),
    ("top", "swap", ""),
    ("top", "reset", ""),
    ("top", "misc", ""),
    ("rename", "edit", "Enter"),
    ("swap", "swap2", "Enter"),
    ("misc", "battery", ""),
    ("misc", "disp", ""),
    ("misc", "quiet", ""),
    ("misc", "boot", ""),
    ("boot", "bootsel", "Enter"),
]


def flow_box(node):
    col, row = FLOW_NODES[node][0], FLOW_NODES[node][1]
    x, w = FLOW_COLS[col]
    return x, FLOW_ROW0 + FLOW_PITCH * row, w


def build_flow_svg():
    parts = []
    # エッジを先に描いて箱の下へ回す
    for src, dst, label in FLOW_EDGES:
        x1, y1, w1 = flow_box(src)
        x2, y2, _ = flow_box(dst)
        sx = x1 + w1
        mid = (sx + x2) / 2
        parts.append(
            f'<path class="edge" d="M{sx} {y1} H{mid:.0f} V{y2} H{x2 - 7}" '
            f'marker-end="url(#flow-arrow)"/>')
        if label:
            # ラベルは箱と箱の隙間の中央に置く（箱に重ねない）
            parts.append(
                f'<text class="edge-label" x="{(sx + x2) / 2:.0f}" y="{y1 - 6}" '
                f'text-anchor="middle">{label}</text>')

    for node, (_, _, title, sub, kind) in FLOW_NODES.items():
        x, y, w = flow_box(node)
        parts.append(
            f'<rect class="box box-{kind}" x="{x}" y="{y - FLOW_BOX_H // 2}" '
            f'width="{w}" height="{FLOW_BOX_H}" rx="7"/>')
        parts.append(f'<text class="box-title" x="{x + 11}" y="{y - 3}">{title}</text>')
        parts.append(f'<text class="box-sub" x="{x + 11}" y="{y + 12}">{sub}</text>')

    body = "\n        ".join(parts)
    return f"""<svg class="flow-svg" viewBox="0 0 {FLOW_W} {FLOW_H}" role="img"
           aria-label="コンフィグメニューの画面遷移">
        <defs>
          <marker id="flow-arrow" viewBox="0 0 8 8" refX="7" refY="4"
                  markerWidth="7" markerHeight="7" orient="auto">
            <path class="arrow-head" d="M0 0 L8 4 L0 8 z"/>
          </marker>
        </defs>
        {body}
      </svg>"""


STATUS_CARDS = "".join([
    screen_card(
        "01_status_bt_named", "BLE接続中（名前あり）", "",
        "<p>スロットに名前を付けている場合は <code>BLE rename</code> で設定した名前を表示する。右端はバッテリー残量アイコン（この例は満充電）。</p>",
    ),
    screen_card(
        "02_status_bt_unnamed", "BLE接続中（名前未設定）", "",
        "<p>名前が空のスロットは <code>BT1</code>〜<code>BT6</code> を表示する。基板の配線都合で内部のスロット番号とは逆順（内部スロット0が <code>BT6</code>、内部スロット5が <code>BT1</code>）だが、一覧は常に上から <code>BT1</code>→<code>BT6</code> の昇順に並ぶ。</p>",
    ),
    screen_card(
        "03_status_usb", "USB接続中", "",
        "<p>BT を無効化して USB 専用にすると <code>USB</code> 表示になる。右端は USB アイコン（トライデント）と、その右に充電中を示す矢印。USB ホストとして認識された瞬間に BT は自動で OFF になる（充電器のみでは発動しない）。</p>",
    ),
    screen_card(
        "04_status_pairing", "ペアリング中", "",
        "<p><code>_FN2</code>+<code>Space</code>（または <code>Pairing</code> メニュー）でオープン広告を始めると <code>PAIR</code> と表示する。ペアリングは Just Works なので、パスキーの表示・入力は無い（下記「既知の制限」を参照）。</p>",
    ),
    screen_card(
        "05_status_disconnected", "未接続", "",
        "<p>BT 有効だがどのホストにも接続していない、かつペアリング中でもない状態。<code>---</code> を表示する。</p>",
    ),
    screen_card(
        "06_status_mac_mode", "Macモード ON", "",
        "<p>接続中のスロットが Mac モードのときだけ、バッテリーアイコンの左に <code>M</code> を出す。Mac モード中は <code>Ctrl</code> と <code>GUI(Win)</code> の usage を入れ替えて送信する。</p>",
    ),
    screen_card(
        "07_status_leds", "LEDインジケータ点灯", "",
        "<p>ホストが通知した Lock 状態を下段に <code>NUM</code> / <code>CAP</code> / <code>SCR</code> の固定位置で表示する（消灯中の項目は描かれない）。本機に物理 LED は無い。USB・BLE どちらの接続でも Output レポートを受け取って反映する。</p>",
    ),
])

MENU_CARDS = "".join([
    screen_card(
        "10_menu_top", "トップメニュー", "FN2+End で起動",
        "<p><code>_FN2</code>+<code>End</code> で開く最初の画面。<code>↑</code>/<code>↓</code> で <code>&gt;</code> カーソルを動かし、<code>Enter</code> で決定する。7 項目あるが 1 画面は 4 行なので、入りきらない分はカーソルに追従してスクロールする。</p>"
        "<ul class=\"item-list\"><li>Connect — 接続先の切り替え</li><li>Pairing — ペアリングの開始 / 中止</li><li>Mac mode — スロットごとの Ctrl/GUI 入れ替え</li><li>BLE rename — スロット名の変更</li><li>BLE swap — スロットの中身の入れ替え</li><li>BLE reset — ボンドの削除</li><li>Misc — Battery / Display / Quiet / Boot mode</li></ul>",
    ),
    screen_card(
        "11_menu_top_scrolled", "トップメニュー（スクロール後）", "↓ で下端へ",
        "<p>カーソルが表示窓の下端に達すると 1 行ずつ送られる（窓の先頭はカーソルの 1 つ上に張り付く）。カーソルは末尾で反対側へループするので、開いた直後に <code>↑</code> を 1 回押すだけでも最下段の <code>Misc</code> に届く。</p>",
    ),
    screen_card(
        "12_menu_connect", "Connect（接続先選択）", "Enter で切替",
        "<p>先頭が <code>USB</code>、続いて <code>BT1</code>〜<code>BT6</code>。取り消し線はボンドが無い（未ペアリング）スロット、右端の <code>(C)</code> は現在の接続先を示す。取り消し線のスロットと接続中のスロットは選んでも何も起きない。切り替えを決定するとメニューは閉じる。</p>",
    ),
    screen_card(
        "13_menu_macmode", "Mac mode（スロットごと）", "Enter でトグル",
        "<p>行の右端の <code>ON</code> / <code>OFF</code> が現在値。<code>Enter</code> で切り替えても画面は開いたままなので、続けて他のスロットも変更できる。値はスロットごとにフラッシュへ保存される。</p>",
    ),
    screen_card(
        "14_menu_rename", "BLE rename（スロット選択）", "Enter で編集へ",
        "<p>名前を変更するスロットを選ぶ。名前が設定済みのスロットは <code>BT1 MacBookPro</code> のように現在の名前を添えて表示する。</p>",
    ),
    screen_card(
        "15_menu_edit", "名前入力", "Enter:確定 Esc:戻る",
        "<p>英数字・空白・<code>-</code>（Shift で <code>_</code>）を最大 10 文字入力できる。末尾の <code>_</code> は入力カーソル、<code>Backspace</code> で 1 文字削除する。<code>Enter</code> で確定して一覧へ戻り、<code>Esc</code> は保存せずに一覧へ戻る。空のまま確定すると名前を削除する。</p>",
    ),
    screen_card(
        "16_menu_swap", "BLE swap（入れ替え元）", "Enter で次へ",
        "<p>スロットの中身（ボンド・名前・Mac モード）をまるごと入れ替える機能。まず入れ替え元を選ぶ。表示は <code>BLE reset</code> と同じで、取り消し線が未ペアリング、<code>(C)</code> が接続中。</p>",
    ),
    screen_card(
        "17_menu_swap_target", "BLE swap（入れ替え先）", "Enter で実行",
        "<p>入れ替え元のスロットには <code>(F)</code> が付く。別のスロットを選ぶとその場で入れ替え、入れ替え元の選択へ戻る（続けて別の組も入れ替えられる）。どちらかに接続中だった場合は切断される。</p>",
    ),
    screen_card(
        "18_menu_reset", "BLE reset（ボンド削除）", "Enter で削除",
        "<p>スロットを選んで <code>Enter</code> を押すとそのボンドを削除する（確認画面は無い）。取り消し線はすでに未ペアリングであることを示す。画面は閉じないので、続けて他のスロットも消せる。</p>",
    ),
    screen_card(
        "19_menu_misc", "Misc", "Esc で Top へ",
        "<p>接続に関係しない項目をまとめた画面。<code>Display</code> と <code>Quiet</code> は現在値でラベルが入れ替わる（消灯中は <code>Display On</code>、Quiet 有効中は <code>Quiet Off</code>）。どちらも選ぶとメニューを閉じる。</p>"
        "<ul class=\"item-list\"><li>Battery — 電圧と残量の表示</li><li>Display Off / On — 手動消灯の切り替え</li><li>Quiet On / Off — 内蔵 DC-DC を低ノイズ側へ固定</li><li>Boot mode — 書き込みモードへ再起動</li></ul>",
    ),
    screen_card(
        "20_menu_battery", "Battery（電圧表示）", "Esc で Misc へ",
        "<p>実測電圧とバッテリー残量の概算 % を表示するだけの画面（リストではない）。充電中は充電電圧を見てしまうため実際より高く出る点に注意。</p>",
    ),
    screen_card(
        "21_menu_boot", "Boot mode（確認）", "Enter:実行 Esc:キャンセル",
        "<p><code>Enter</code> で BOOTSEL（UF2 書き込み）モードへ即座に再起動する。破壊的操作のため <code>Misc</code> の最後尾に置かれている。</p>",
    ),
    screen_card(
        "22_booting", "再起動直前", "",
        "<p>ブートローダへ落ちる直前に <code>Booting...</code> を中央表示する。省電力でパネルが消えていても見えるよう、この間だけ点灯を強制する。</p>",
    ),
])

ICON_ROWS = "".join([
    icon_row("icon_batt_5", "battery 5/5", "5/5", "75% 以上"),
    icon_row("icon_batt_4", "battery 4/5", "4/5", "50% 以上"),
    icon_row("icon_batt_2", "battery 2/5", "2/5", "25% 以上"),
    icon_row("icon_batt_0", "battery 0/5", "0/5", "25% 未満"),
    icon_row("icon_charge", "charging", "充電中", "USB 接続中に右端へ表示する矢印アイコン"),
    icon_row("icon_usb", "usb", "USB", "USB 接続中に表示するトライデント（矢印の左隣）"),
])

MARKER_ROWS = "".join([
    f'<tr><td class="key">{sym}</td><td>{where}</td><td>{desc}</td></tr>'
    for sym, where, desc in [
        ("&gt;", "一覧画面の左端", "カーソル位置。<code>↑</code>/<code>↓</code> で動かす"),
        ("(C)", "Connect / BLE reset / BLE swap", "現在接続しているスロット"),
        ("(F)", "BLE swap（入れ替え先）", "先に選んだ入れ替え元のスロット"),
        ("ON / OFF", "Mac mode", "そのスロットの Mac モードの現在値"),
        ("取り消し線", "Connect / BLE reset / BLE swap", "ボンドが無い（未ペアリング）スロット"),
        ("M", "ステータス画面の右上", "接続中スロットが Mac モード"),
        ("_", "名前入力", "文字入力カーソル"),
    ]
])

HTML = f"""<!doctype html><meta charset="utf-8">
<title>EhHHKB2 Menu Guide</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;500;700&family=Public+Sans:wght@400;500;600;700&display=swap" rel="stylesheet">
<style>
:root {{
  --bg: #f4f2ee;
  --surface: #ffffff;
  --surface-2: #ebe7e0;
  --ink: #1c1f22;
  --muted: #5c6570;
  --accent: #0f8b8d;
  --accent-ink: #ffffff;
  --border: #dcd8d0;
  --code-bg: #ece9e2;
  --shadow: 0 1px 2px rgba(28,31,34,0.06), 0 8px 24px rgba(28,31,34,0.05);
}}
@media (prefers-color-scheme: dark) {{
  :root:not([data-theme="light"]) {{
    --bg: #0c0f12;
    --surface: #14181c;
    --surface-2: #1b2126;
    --ink: #e7ecef;
    --muted: #8a97a0;
    --accent: #57e6d9;
    --accent-ink: #06201e;
    --border: #232a30;
    --code-bg: #1b2126;
    --shadow: 0 1px 2px rgba(0,0,0,0.4), 0 8px 24px rgba(0,0,0,0.35);
  }}
}}
:root[data-theme="dark"] {{
  --bg: #0c0f12;
  --surface: #14181c;
  --surface-2: #1b2126;
  --ink: #e7ecef;
  --muted: #8a97a0;
  --accent: #57e6d9;
  --accent-ink: #06201e;
  --border: #232a30;
  --code-bg: #1b2126;
  --shadow: 0 1px 2px rgba(0,0,0,0.4), 0 8px 24px rgba(0,0,0,0.35);
}}

* {{ box-sizing: border-box; }}
html {{ scroll-behavior: smooth; }}
body {{
  margin: 0;
  background: var(--bg);
  color: var(--ink);
  font-family: "Public Sans", "Hiragino Sans", "Yu Gothic", sans-serif;
  line-height: 1.65;
  font-size: 16px;
}}
code {{
  font-family: "JetBrains Mono", ui-monospace, monospace;
  background: var(--code-bg);
  padding: 0.1em 0.4em;
  border-radius: 4px;
  font-size: 0.88em;
}}

.layout {{
  display: grid;
  grid-template-columns: 240px minmax(0, 1fr);
  gap: 0;
  max-width: 1180px;
  margin: 0 auto;
}}

/* ---- サイドバー ---- */
.sidebar {{
  position: sticky;
  top: 0;
  align-self: start;
  height: 100vh;
  overflow-y: auto;
  padding: 2rem 1.25rem;
  border-right: 1px solid var(--border);
}}
.brand {{
  font-family: "JetBrains Mono", monospace;
  font-weight: 700;
  font-size: 0.95rem;
  letter-spacing: 0.02em;
  margin: 0 0 0.2rem;
}}
.brand-sub {{
  color: var(--muted);
  font-size: 0.78rem;
  margin: 0 0 1.75rem;
}}
.toc {{ list-style: none; margin: 0; padding: 0; }}
.toc-group {{
  font-family: "JetBrains Mono", monospace;
  font-size: 0.68rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--muted);
  margin: 1.4rem 0 0.5rem;
}}
.toc-group:first-child {{ margin-top: 0; }}
.toc a {{
  display: block;
  color: var(--ink);
  text-decoration: none;
  font-size: 0.88rem;
  padding: 0.32rem 0;
  border-left: 2px solid transparent;
  padding-left: 0.7rem;
  margin-left: -0.7rem;
  opacity: 0.92;
}}
.toc a:hover {{ opacity: 1; border-left-color: var(--accent); color: var(--accent); }}

/* ---- メイン ---- */
main {{ padding: 2.5rem 3rem 6rem; min-width: 0; }}
.hero h1 {{
  font-family: "JetBrains Mono", monospace;
  font-size: clamp(1.6rem, 2.6vw, 2.1rem);
  line-height: 1.25;
  margin: 0 0 0.6rem;
  text-wrap: balance;
}}
.hero p.lede {{
  color: var(--muted);
  max-width: 62ch;
  font-size: 1.02rem;
  margin: 0 0 1.6rem;
}}
.fact-strip {{
  display: flex;
  flex-wrap: wrap;
  gap: 0.6rem;
  margin-bottom: 2.6rem;
}}
.fact {{
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 0.55rem 0.85rem;
  font-family: "JetBrains Mono", monospace;
  font-size: 0.8rem;
  color: var(--muted);
}}
.fact b {{ color: var(--ink); font-weight: 600; }}

section {{ margin: 3.2rem 0; scroll-margin-top: 1.5rem; }}
section > h2 {{
  font-family: "JetBrains Mono", monospace;
  font-size: 1.05rem;
  letter-spacing: 0.01em;
  margin: 0 0 0.3rem;
  padding-bottom: 0.6rem;
  border-bottom: 1px solid var(--border);
}}
section > h3 {{
  font-size: 0.95rem;
  margin: 2.2rem 0 0.2rem;
}}
section > p.section-lede {{
  color: var(--muted);
  max-width: 66ch;
  margin: 0.8rem 0 1.4rem;
}}

/* ---- 操作テーブル ---- */
table.keys {{
  width: 100%;
  border-collapse: collapse;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 10px;
  overflow: hidden;
  box-shadow: var(--shadow);
}}
table.keys th, table.keys td {{
  text-align: left;
  padding: 0.65rem 1rem;
  border-bottom: 1px solid var(--border);
  font-size: 0.92rem;
}}
table.keys th {{
  font-family: "JetBrains Mono", monospace;
  font-size: 0.72rem;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  color: var(--muted);
  background: var(--surface-2);
}}
table.keys tr:last-child td {{ border-bottom: none; }}
table.keys td.key {{ font-family: "JetBrains Mono", monospace; color: var(--accent); white-space: nowrap; }}
.table-wrap {{ overflow-x: auto; }}

/* ---- 画面カード ---- */
.grid {{
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
  gap: 1.1rem;
}}
.card {{
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 14px;
  overflow: hidden;
  box-shadow: var(--shadow);
  display: flex;
  flex-direction: column;
}}
.oled {{
  background: #0a0d10;
  padding: 14px 14px 16px;
  display: flex;
  justify-content: center;
  position: relative;
}}
.oled::before {{
  content: "";
  position: absolute;
  inset: 6px;
  border-radius: 6px;
  box-shadow: inset 0 0 0 1px rgba(255,255,255,0.06);
  pointer-events: none;
}}
.screen-img {{
  width: 100%;
  max-width: 360px;
  height: auto;
  image-rendering: pixelated;
  border-radius: 3px;
  background: #04080d;
  box-shadow: 0 0 14px rgba(87, 230, 217, 0.18);
}}
.card-body {{ padding: 1rem 1.15rem 1.2rem; }}
.card-head {{
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 0.6rem;
  margin-bottom: 0.35rem;
}}
.card-head h3 {{
  font-size: 1rem;
  margin: 0;
  font-weight: 600;
}}
.kbd-hint {{
  font-family: "JetBrains Mono", monospace;
  font-size: 0.74rem;
  color: var(--accent);
  white-space: nowrap;
}}
.card-body p {{ margin: 0 0 0.4rem; font-size: 0.9rem; color: var(--ink); }}
.card-body .item-list {{
  margin: 0.5rem 0 0;
  padding-left: 1.1rem;
  font-size: 0.88rem;
  color: var(--ink);
}}
.card-body .item-list li {{ margin: 0.15rem 0; }}

/* ---- アイコン凡例 ---- */
.icon-legend {{
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 12px;
  box-shadow: var(--shadow);
  padding: 0.4rem 1.2rem;
  margin-bottom: 1.6rem;
}}
.icon-row {{
  display: flex;
  align-items: center;
  gap: 1rem;
  padding: 0.75rem 0;
  border-bottom: 1px solid var(--border);
}}
.icon-row:last-child {{ border-bottom: none; }}
.icon-frame {{
  flex: none;
  width: 72px;
  height: 72px;
  background: #0a0d10;
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
}}
.icon-img {{
  width: 48px;
  height: 64px;
  image-rendering: pixelated;
}}
.icon-text {{ display: flex; flex-direction: column; gap: 0.1rem; }}
.icon-label {{ font-family: "JetBrains Mono", monospace; font-weight: 600; font-size: 0.9rem; }}
.icon-desc {{ color: var(--muted); font-size: 0.86rem; }}

/* ---- 遷移図（インライン SVG） ---- */
.flow {{
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 12px;
  box-shadow: var(--shadow);
  padding: 1.2rem;
  overflow-x: auto;
}}
.flow-svg {{ width: 100%; min-width: 780px; height: auto; display: block; }}
.flow-svg .box {{ fill: var(--surface-2); stroke: var(--muted); stroke-opacity: 0.5; stroke-width: 1; }}
.flow-svg .box-close {{ stroke-opacity: 0.85; stroke-dasharray: 4 3; }}
.flow-svg .box-term {{ fill: none; stroke: var(--accent); stroke-opacity: 1; stroke-width: 1.5; }}
.flow-svg text {{ font-family: "Public Sans", "Hiragino Sans", "Yu Gothic", sans-serif; }}
.flow-svg .box-title {{ fill: var(--ink); font-size: 13px; font-weight: 600; }}
.flow-svg .box-sub {{ fill: var(--muted); font-size: 10.5px; }}
.flow-svg .edge {{ fill: none; stroke: var(--muted); stroke-opacity: 0.55; stroke-width: 1.4; }}
.flow-svg .edge-label {{ fill: var(--muted); font-family: "JetBrains Mono", monospace; font-size: 10.5px; }}
.flow-svg .arrow-head {{ fill: var(--muted); fill-opacity: 0.75; }}
.flow-note {{ color: var(--ink); font-size: 0.88rem; margin: 0.9rem 0 0; max-width: 66ch; }}

/* ---- フォント一覧 ---- */
.font-sheet {{
  background: #0a0d10;
  border: 1px solid var(--border);
  border-radius: 12px;
  padding: 0.8rem;
  box-shadow: var(--shadow);
}}
.font-sheet img {{ width: auto; max-width: 100%; height: auto; display: block; margin: 0 auto; image-rendering: pixelated; }}

/* ---- 注意事項 ---- */
.notes {{
  background: var(--surface);
  border: 1px solid var(--border);
  border-left: 3px solid var(--accent);
  border-radius: 0 10px 10px 0;
  padding: 1rem 1.3rem;
}}
.notes ul {{ margin: 0; padding-left: 1.2rem; }}
.notes li {{ margin: 0.5rem 0; font-size: 0.92rem; color: var(--ink); }}
.notes li b {{ color: var(--ink); font-weight: 600; }}

footer {{
  max-width: 1180px;
  margin: 0 auto;
  padding: 0 3rem 3rem;
  color: var(--muted);
  font-size: 0.78rem;
  font-family: "JetBrains Mono", monospace;
}}

@media (max-width: 860px) {{
  .layout {{ grid-template-columns: 1fr; }}
  .sidebar {{
    position: static;
    height: auto;
    border-right: none;
    border-bottom: 1px solid var(--border);
    padding: 1.4rem 1.25rem;
  }}
  main {{ padding: 2rem 1.25rem 4rem; }}
  footer {{ padding: 0 1.25rem 2rem; }}
}}
</style>

<div class="layout">
  <nav class="sidebar">
    <p class="brand">EhHHKB2</p>
    <p class="brand-sub">メニュー操作ガイド</p>
    <ul class="toc">
      <li class="toc-group">はじめに</li>
      <li><a href="#start">起動と基本操作</a></li>
      <li class="toc-group">通常表示</li>
      <li><a href="#status">ステータス画面</a></li>
      <li class="toc-group">コンフィグメニュー</li>
      <li><a href="#flow">画面遷移</a></li>
      <li><a href="#menu">各画面の詳細</a></li>
      <li class="toc-group">リファレンス</li>
      <li><a href="#icons">アイコンとマーカー</a></li>
      <li><a href="#font">内蔵フォント</a></li>
      <li><a href="#notes">既知の制限</a></li>
    </ul>
  </nav>

  <main>
    <div class="hero">
      <h1>EhHHKB2 メニュー操作ガイド</h1>
      <p class="lede">
        128×32 の OLED に表示される全画面と、内蔵 6×8 ドットフォントの見え方を
        実際のフォントデータから再現したプレビューでまとめたガイド。
        キーボードだけで接続先の切り替え・ボンド管理・名前変更ができる。
      </p>
      <div class="fact-strip">
        <span class="fact">ディスプレイ <b>SSD1306 128×32</b></span>
        <span class="fact">フォント <b>6×8（5×7 + 字間1px）</b></span>
        <span class="fact">メニュー起動 <b>FN2 + End</b></span>
        <span class="fact">表示行数 <b>最大4行</b></span>
        <span class="fact">BLEスロット <b>6</b></span>
      </div>
    </div>

    <section id="start">
      <h2>起動と基本操作</h2>
      <p class="section-lede">
        <code>_FN2</code>（<code>FN</code>+<code>Esc</code> 押下中）+ <code>End</code> でコンフィグメニューを開く。
        メニュー表示中は HID 送信を止めるため、操作キーがホストに漏れることはない。
      </p>
      <table class="keys">
        <thead><tr><th>キー</th><th>動作</th></tr></thead>
        <tbody>
          <tr><td class="key">↑ / ↓</td><td>カーソルを移動する（末尾で反対側へループする）</td></tr>
          <tr><td class="key">Enter</td><td>選択項目を決定する。名前入力画面ではその場で確定する</td></tr>
          <tr><td class="key">Esc</td><td>ひとつ前の画面へ戻る（<code>Battery</code> / <code>Boot mode</code> は <code>Misc</code> へ戻る）。トップメニューで押すとメニューを終了する</td></tr>
          <tr><td class="key">A–Z, 0–9, Space, - / _</td><td>名前入力画面でのみ有効。<code>Backspace</code> で1文字削除</td></tr>
        </tbody>
      </table>

      <h3>メニューを開かずに使うショートカット</h3>
      <p class="section-lede">よく使う操作は <code>_FN2</code> レイヤーへ直接割り当ててある。</p>
      <table class="keys">
        <thead><tr><th>キー</th><th>動作</th></tr></thead>
        <tbody>
          <tr><td class="key">_FN2 + 1–6</td><td>BLE スロット <code>BT1</code>–<code>BT6</code> へ接続を切り替える</td></tr>
          <tr><td class="key">_FN2 + 7</td><td>BT を無効化して USB 専用にする</td></tr>
          <tr><td class="key">_FN2 + Space</td><td>ペアリング開始 / 中止</td></tr>
          <tr><td class="key">_FN2 + Z</td><td>全スロットのボンドと名前を削除する</td></tr>
          <tr><td class="key">_FN2 + End</td><td>コンフィグメニューを開く</td></tr>
        </tbody>
      </table>
    </section>

    <section id="status">
      <h2>ステータス画面</h2>
      <p class="section-lede">
        メニューを開いていないときの通常表示。上段に接続先、右端にバッテリー残量、
        下段にホストから通知された Lock キーの状態を表示する。
      </p>
      <div class="grid">{STATUS_CARDS}
      </div>
    </section>

    <section id="flow">
      <h2>画面遷移</h2>
      <p class="section-lede">トップメニューから各機能画面への遷移。<code>Esc</code> は逆方向へ一段ずつ戻る。</p>
      <div class="flow">
        {build_flow_svg()}
      </div>
      <p class="flow-note">
        点線の枠は、決定するとメニューを閉じて通常表示へ戻る項目。
        <code>Mac mode</code> / <code>BLE reset</code> / 入れ替え先 は決定後も画面に留まるので、
        続けて別のスロットを操作できる。
      </p>
    </section>

    <section id="menu">
      <h2>各画面の詳細</h2>
      <p class="section-lede">
        トップメニューの7項目と、その先の子画面。取り消し線・<code>(C)</code>・<code>(F)</code>・
        カーソルの<code>&gt;</code>は実機の描画ロジックをそのまま再現している。
      </p>
      <div class="grid">{MENU_CARDS}
      </div>
    </section>

    <section id="icons">
      <h2>アイコンとマーカー</h2>
      <p class="section-lede">
        右上に表示されるインジケータ用グリフ。ASCII 印字可能文字（0x20–0x7E）の続きに
        独自グリフを追加している。
      </p>
      <div class="icon-legend">{ICON_ROWS}
      </div>
      <div class="table-wrap">
        <table class="keys">
          <thead><tr><th>表示</th><th>場所</th><th>意味</th></tr></thead>
          <tbody>{MARKER_ROWS}</tbody>
        </table>
      </div>
    </section>

    <section id="font">
      <h2>内蔵フォント</h2>
      <p class="section-lede">
        <code>display/ssd1306.c</code> の <code>font_data[]</code> 全 103 文字（0x20–0x86）。
        1 文字は 5×7 ドット + 字間 1px の 6×8 で、1 行は最大 21 文字・1 画面は 4 行。
        <code>0x7F</code>（BTマーク）と <code>0x80</code>（✕印）はフォントには入っているが、
        現在の画面描画では使っていない。
      </p>
      <div class="font-sheet">
        <img src="data:image/png;base64,{b64('font_sheet')}" alt="6x8 フォント一覧">
      </div>
    </section>

    <section id="notes">
      <h2>既知の制限</h2>
      <div class="notes">
        <ul>
          <li><b>バッテリー残量は電圧からの概算</b> — 充電中は充電電圧を見てしまうため、実際より高く表示される。
            残量アイコン・<code>Battery</code>画面・BLE通知はすべて同じ換算 % を参照するため、この誤差も含めて表示は一致する</li>
          <li><b>ペアリング中に既知デバイスから接続要求が来たら無視する</b> — 切断して待機を続けるため、
            同じホストと再ペアリングするには先にそのスロットを <code>BLE reset</code> する</li>
          <li><b>ペアリング中にパスキーは表示されない</b> — <code>ble/ble_hid.c</code> が
            <code>IO_CAPABILITY_NO_INPUT_NO_OUTPUT</code>（MITM 要求なし）で初期化するため、
            ペアリングは常に Just Works になる。<code>ui/screen.c</code> には
            <code>Code:######</code> を描く経路があるが、現在の設定では到達しない</li>
          <li><b>Quiet mode は電池持ちとのトレードオフ</b> — 内蔵 DC-DC を低ノイズ側へ固定するため、
            軽負荷時の効率が落ちる。ノイズが気になる場面だけ有効にする</li>
          <li><b>表示できる文字数の上限</b> — 1 行 21 文字・4 行まで。スロット名は 10 文字までで、
            それを超える入力は受け付けない</li>
        </ul>
      </div>
    </section>
  </main>
</div>

<footer>display/ssd1306.c の font_data[] と ui/screen.c の描画ロジックから生成したプレビュー</footer>
"""


def main():
    out_path = os.path.join(OUT_DIR, "manual.html")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(HTML)
    print("wrote", out_path)


if __name__ == "__main__":
    main()
