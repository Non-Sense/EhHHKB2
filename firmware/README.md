# EhHHKB2 Firmware

Raspberry Pi Pico 2 W（RP2350 + CYW43）向けの自作キーボードファームウェア。
USB HID と BLE HID（HID over GATT）の両対応で、BLE は 6 ホストを記憶して切り替えられる。
128x32 の OLED に接続状態・バッテリー・LED インジケータを表示し、キーボードだけで設定変更ができる。

## 主な機能

- 8×11 キーマトリクス（88 交点のうち 85 キー）、0.5ms 周期スキャン・押下優先デバウンス
- 3 レイヤー（`_BASE` / `_FN` / `_FN2`）。押下した瞬間のキーコードをラッチするので、
  修飾を先に離してもキーが化けない
- 128bit bitmap レポートによる NKRO。ホストが Boot Protocol を要求したら 6KRO へ自動フォールバック
- Consumer Control による メディアキー（音量・再生・曲送り等）。USB / BLE 両対応
- BLE ホストスロット 6 個。ボンド・スロット名・Mac モードをフラッシュに永続化し、ホワイトリスト広告で切り替える
- スロットごとの Mac モード（`Ctrl` と `GUI(Win)` の usage を入れ替えて送信する）
- 出力先は「最後に接続が確立した方」のみ。USB と BLE へ同時送信はしない
- SSD1306 OLED への状態表示と、キーボードから操作するコンフィグメニュー
- バッテリー電圧の ADC 監視と、バッテリー駆動時の自動消灯

## ハードウェア

| 機能 | GPIO |
| --- | --- |
| キーマトリクス row（入力・プルアップ） | 2–9 |
| キーマトリクス col（出力・スキャン時のみ LOW） | 10–20 |
| OLED I2C0 SDA | 0 |
| OLED I2C0 SCL | 1 |
| USB センス（初期化のみ・未使用） | 22 |
| バッテリー電圧（ADC2 / 1/2 分圧） | 28 |

- ボード: `pico2_w`
- OLED: SSD1306 128x32、I2C アドレス `0x3C`、400kHz
- stdio（UART / USB CDC）は無効

## ビルド

Pico SDK 2.2.0 と arm toolchain 14_2_Rel1 を前提にしている（`~/.pico-sdk` 配下、または `PICO_SDK_PATH`）。

```powershell
cmake -S . -B build
cmake --build build
```

生成物は `build/ehhhkb2.uf2`。BOOTSEL を押しながら接続してドラッグ&ドロップするか、
コンフィグメニューの `Boot mode` から書き込みモードへ再起動できる。

### ホスト単体テスト

ハードウェアに触れないロジック（`ui/config_menu.c` と `hid/hid_report.c`）は PC 上で実行できる。

```powershell
cmake -S tests -B build-tests
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

### ドキュメント生成

`scripts/` に、OLED の全画面を実機と同じピクセル配置で PNG 化し、それを埋め込んだ
メニュー操作ガイド（`scripts/out/manual.html`、1 枚の自己完結 HTML）を組み立てる
スクリプトがある。`render_screens.py` は `display/ssd1306.c` の `font_data[]` と
`ui/screen.c` の描画ロジックを Python へ移植したものなので、**フォントや画面描画・
メニュー構成を変えたら両方を追従させて再生成する。**

```powershell
python -m venv scripts/.venv
scripts/.venv/Scripts/python.exe -m pip install -r scripts/requirements.txt
scripts/.venv/Scripts/python.exe scripts/render_screens.py   # scripts/out/*.png
scripts/.venv/Scripts/python.exe scripts/build_manual.py     # scripts/out/manual.html
```

## キーマップ

配列定義は `hid/key_matrix.c` の `keymap`、物理配列と行列の対応は `hid/key_matrix.h` の `LAYOUT` マクロ。

`_FN` は FN キー（右 Shift 隣 / 左下）の押下中、`_FN2` は FN + Esc の押下中に有効になる。
透過キーは `_FN2` → `_FN` → `_BASE` の順にフォールスルーする。

### `_FN`（FN 押下中）

| 押すキー | 出力 |
| --- | --- |
| `1`–`0` / `-` / `=` | F1–F10 / F11 / F12 |
| `` ` `` / `\` | Delete / Insert |
| `Tab` | CapsLock |
| `I` / `O` / `P` | PrintScreen / ScrollLock / Pause |
| `[` / `;` / `'` / `/` | ↑ / ← / → / ↓ |
| `K` / `,` / `L` / `.` | Home / End / PageUp / PageDown |
| `PageUp` / `PageDown` | 音量アップ / 音量ダウン（メディアキー） |
| `Esc` | `_FN2` レイヤーへ |

### `_FN2`（FN + Esc 押下中）

| 押すキー | 動作 |
| --- | --- |
| `1`–`6` | BLE スロット `BT1`–`BT6` へ接続を切り替える |
| `7` | BT を無効化して USB 専用にする |
| `Space` | ペアリング開始（オープン広告で新規デバイスを待つ）。ペアリング中に押すと離脱する |
| `Z` | 全スロットのボンドと名前を削除して待機 |
| `End` | コンフィグメニューを開く |

## BLE

- 広告名 `EhHHKB2` / Appearance `0x03C1`（キーボード） / HID Service `0x1812`
- ペアリングは Just Works（`IO_CAPABILITY_NO_INPUT_NO_OUTPUT`、Secure Connections + Bonding）
- Battery Service は実測電圧を Li-Po の放電曲線で 0–100% に換算して通知する。
  値が変わったときだけ、かつ 60 秒以上の間隔をおいて送る
- スロット番号は `le_device_db` のインデックスに直結する。上限は 6（`MAX_NR_LE_DEVICE_DB_ENTRIES` / `BLE_HOST_SLOTS`）
- スロット名（最大 10 文字）は BTstack の TLV フラッシュに保存される

### ホスト切り替えの挙動

対象スロットのボンドをホワイトリストに載せて広告することで、直前まで接続していたホスト OS の
即時再接続を弾いている。RPA を使うホストでも、コントローラのリゾルビングリストへ IRK を
読み込めていれば照合が通る。読み込めていない場合と、未ペアリングスロットへの切り替え時は
オープン広告へフォールバックする。

USB ホストとして enumerate した立ち上がりでは BT を自動的に OFF にする。
充電器のみの接続では発動しない。ケーブルを抜いても自動復帰はせず、`_FN2` の BT キーか
ペアリング操作で手動復帰する。

## コンフィグメニュー

`_FN2` + `End` で起動。↑↓ で移動、Enter で決定、Esc で戻る（トップで Esc なら終了）。
メニュー表示中は HID 送信を止め、押下中のキーはホストへ漏れない。

トップは 7 項目。接続に関係しない項目は `Misc` へまとめてある。

| 項目 | 内容 |
| --- | --- |
| `Connect` | `USB` / BT1–6 を選んで接続先を切り替える。取り消し線は未ペアリング、`(C)` が接続中 |
| `Pairing` | ペアリングモードへ入る（オープン広告で新規デバイスを待つ）。ペアリング中は `CancelPair` と表示され、選ぶと離脱する |
| `Mac mode` | スロットごとに Ctrl/GUI(Win) を入れ替えて送信する。行の右端が現在値（`ON` / `OFF`）で、決定しても画面は閉じない |
| `BLE rename` | スロット名を入力する。英数字・空白・`-`（Shift で `_`）、最大 10 文字 |
| `BLE swap` | 入れ替え元→入れ替え先の順にスロットを選び、ボンド・名前・Mac モードをまとめて入れ替える。入れ替え先画面では選択元に `(F)` を表示する。完了後も画面は閉じず、続けて別の組を入れ替えられる |
| `BLE reset` | スロットごとにボンドを削除する |
| `Misc` | 下記のサブメニューへ入る |

`Misc` の項目。`Battery` / `Boot mode` の画面から `Esc` で戻る先は `Misc`（それ以外の画面はトップへ戻る）。

| 項目 | 内容 |
| --- | --- |
| `Battery` | バッテリー電圧と残量 % を表示する |
| `Display Off` | ディスプレイを消灯する。消灯中もメニューを開けば表示は見え、項目は `Display On` に変わる。選び直すと点灯に戻る |
| `Quiet On` | CYW43 の `WL_GPIO1` を High にして内蔵 DC-DC を低ノイズ側へ固定する。有効中は `Quiet Off` と表示。効率は落ちるので電池持ちとのトレードオフ |
| `Boot mode` | BOOTSEL（UF2 書き込み）モードへ再起動する。破壊的操作なので末尾に置いている |

## ディスプレイ

- 上段: 接続先のスロット名（未設定なら `BT1`–`BT6`）/ `USB` / `PAIR` / `---`。
  右端はバッテリー残量アイコン、充電中は矢印と USB アイコン。
  接続中スロットが Mac モードなら残量アイコンの左に `M`
- 下段: ホストが通知した LED 状態を `NUM` / `CAP` / `SCR` で表示（USB / BLE どちらの
  接続でも Output レポートから取得する）。本機に物理 LED はない
- 残量アイコンのしきい値: `Battery` 画面や BLE 通知と同じ換算 % を参照する。75%以上で 5/5、
  50%以上で 4/5、25%以上で 2/5、未満は 0/5

点灯条件（`ui/display_policy.c`）は次のいずれか。

- 起動から 5 秒間
- コンフィグメニュー表示中
- USB 接続中
- `_FN2` レイヤーへ入った瞬間から 10 秒間（wakeup）
- ペアリングのコード表示中から、接続完了 +5 秒まで（後述のとおり Just Works では
  コードが出ないため、この条件は現状成立しない）

## 構成

| ディレクトリ | 役割 |
| --- | --- |
| `hid/` | マトリクススキャン、レイヤー解決、HID レポート組み立て、USB/BLE の出力振り分け |
| `usb/` | TinyUSB デバイススタックとディスクリプタ |
| `ble/` | BTstack による HID over GATT、ホストスロット管理、ボンド・名前の永続化 |
| `display/` | SSD1306 ドライバと 6x8 フォント |
| `power/` | バッテリー電圧の ADC サンプリング |
| `ui/` | コンフィグメニュー状態機械、点灯ポリシー、画面描画 |
| `tests/` | ホスト単体テスト |
| `scripts/` | OLED 画面プレビューとメニュー操作ガイドの生成（「ドキュメント生成」参照） |

### コア分担

- **Core 0**: キースキャン、TinyUSB、BTstack ポーリング、メニュー処理
- **Core 1**: バッテリー監視、OLED 描画

コア間で共有するのは `ui_state_t`（`ui/screen.h`）1 つだけで、critical section 経由で
まるごとスナップショットして受け渡す。Core 1 から BLE / USB / キーマトリクスの状態を
直接読みに行ってはならない。

`ui/config_menu.c` と `ui/display_policy.c` はハードウェアに触らない純粋な状態機械にしてある。
入力はキーレポートと状態のスナップショットだけで、副作用は `cfg_action_t` として呼び出し側へ返す。

### HID レポート形式

レポートは 2 種類あり、Report ID で区別する。

**Report ID 1: キーボード** — modifier 1 バイト + usage `0x00`–`0x7F` の 128bit bitmap（16 バイト）
+ 上位 usage 配列 2 バイト。

- USB: Report ID + 19 バイト
- BLE: Report ID は特性側（Report Reference）が持つので通知は 20 バイト。
  ディスクリプタが予約バイトを宣言しているため、modifier と bitmap の間に 1 バイト挿入する

bitmap は usage `0x7F` までしか表現できないため、`0x80` 以上は 8bit キーコードの配列
（`Report Size 8` / `Report Count 2`）で送る。JIS の `ろ`（`KC_KANJI1`）や `¥`（`KC_KANJI3`）が
これに該当する。同時押しは 2 キーまでで、溢れた分は捨てる。振り分けは
`hid_report_set_key()` が usage を見て自動で行うので、キーマップ側は区別しなくてよい。

BLE の 20 バイトはデフォルト ATT MTU 23 のペイロード上限と一致する。bitmap 自体を
256bit に拡張すると 34 バイトになり、MTU 拡張を negotiate しないホストで送信が
失敗するため、配列方式を選んでいる。

### レポート周期

USB はエンドポイントの `bInterval`（`usb/usb_descriptors.c` の `HID_POLL_INTERVAL_MS`）を
1 にしてあり、フルスピードでは 1ms ポーリング = 1000Hz になる。

キースキャンは `KEY_SCAN_INTERVAL_US`（`ehhhkb2.c`）で 0.5ms 周期。押下はデバウンス待ちが
無いため、押下遅延はスキャン待ち（最悪 0.5ms）+ USB ポーリング（最悪 1ms）で最悪 1.5ms・
平均 0.75ms。USB が 1ms 刻みなのでスキャンをこれ以上速くしても平均 0.125ms しか縮まらず、
フルスピード USB では `bInterval = 1` が構造的な下限になる。

離鍵は `KEY_DEBOUNCE_TICKS`（`hid/key_matrix.c`）ぶんの連続スキャンを待つ。**この値は
スキャン回数なので実時間はスキャン周期に比例する。** 0.5ms × 4 = 2ms で、メカニカル
スイッチのバウンス（1〜5ms）を吸収する下限に置いてある。スキャン周期を変えるときは
必ず両方を比例させること。片方だけ縮めると離鍵のチャタリングが漏れる。

BLE は `bInterval` と無関係で、接続インターバル（ホスト依存で 7.5〜30ms 程度）が
遅延を支配するため、スキャン周期を詰めても効果はない。

**Report ID 2: Consumer Control** — usage を 16bit の配列項目で 1 つ（2 バイト）。離鍵は 0 を送る。
レポートが 1 usage しか運べないため、メディアキーの同時押しは 1 つだけ有効になる。

ホストが Boot Protocol へ切り替えた場合（USB は `SET_PROTOCOL`、BLE は HIDS の Protocol Mode）は、
modifier + 予約 + キーコード 6 個の標準ブートレポートへ変換して送る。ブートプロトコルには
Report ID と Consumer Control が無いので、その間メディアキーは送らない。

### メディアキーの追加

`hid/keycode.h` の 16bit 仮想キーコードは範囲で用途を分けている。

| 範囲 | 用途 |
| --- | --- |
| `0x0000`–`0x00FF` | HID キーボード usage（キーボードレポートへ入れる） |
| `0x0100`–`0x0FFF` | ファームウェア内部のアクション（`KC_BT1` 等。HID へは出さない） |
| `0x1000`–`0x1FFF` | Consumer Control usage（`KC_MEDIA_BASE + usage`） |

定義済みは `KC_MPLAY` / `KC_MSTOP` / `KC_MPREV` / `KC_MNEXT` / `KC_MMUTE` / `KC_MVOLU` / `KC_MVOLD`。
他の usage を使いたい場合は `KC_MEDIA_BASE + 0x00XX` を足すだけでよく、レポート側の変更は不要
（ディスクリプタは usage `0x0000`–`0x03FF` を受け付ける）。

キーボードページの `KC_MUTE` / `KC_VOLUME_UP` / `KC_VOLUME_DOWN` は別物。メディアキーとして
使うなら `KC_M*` の方が確実。

### BLE の Report 特性

BTstack 同梱の `hids.gatt` は Input レポートを Report ID 1 の 1 本しか宣言しないため、
`ble/ehhhkb2.gatt` に自前の HID Service 宣言を置き、Consumer 用の 2 本目（Report ID 2, Input）を
足している。`hids_device_init_with_storage()` が宣言順に `REPORT_REFERENCE` を読んで
(id, type) を登録し、送信は `hids_device_send_input_report_for_id()` で ID を指定する。

Input が 2 本になるので、購読通知（`HIDS_SUBEVENT_INPUT_REPORT_ENABLE`）は report id で
振り分ける必要がある。振り分けないと Consumer の購読でキーボード側の送信可否を壊す。
`can_send_now` は 1 イベントにつき 1 通知しか送れないため、両方に変化があるときは
キーボードを先に送り、残りは次のループで再要求する。

## 既知の制限

- バッテリー残量は電圧からの換算なので、充電中は充電電圧を見てしまい実際より高く出る。
  充電状態を区別する仕組みは無い（OLED の残量アイコン・`Battery` 画面・BLE 通知はすべて
  同じ換算 % を参照するため、この誤差も含めて表示は一致する）
- USB センス（GPIO22）は初期化のみで読み出していない。USB 接続判定は
  `tud_mounted() && !tud_suspended()` で行う（自己給電では VBUS 検出が効かないため）
- ペアリング中に既知デバイスから接続要求が来た場合は切断して無視する。
  再ペアリングするには先にそのスロットを `BLE reset` する
- ペアリングは Just Works（`IO_CAPABILITY_NO_INPUT_NO_OUTPUT` / MITM 要求なし）なので、
  BTstack は `SM_EVENT_PASSKEY_DISPLAY_NUMBER` を上げない。`ui/screen.c` の
  `Code:######` 表示と、それを起点にする `ui/display_policy.c` のペアリング点灯
  ウィンドウは現状どちらも到達しない（ペアリング中の点灯は `_FN2` wakeup の 10 秒頼み）
