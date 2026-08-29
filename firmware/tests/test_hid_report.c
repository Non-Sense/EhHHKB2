// hid/hid_report.c のホスト単体テスト。bitmap（usage 0x00-0x7F）と上位 usage 配列（0x80 以上）の振り分けを検証する。

#include <stdio.h>
#include <string.h>

#include "hid/hid_report.h"
#include "hid/keycode.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                \
    do {                                                           \
        ++checks;                                                  \
        if (!(cond)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                            \
        }                                                          \
    } while (0)

static void test_layout(void) {
    // modifier(1) + bitmap(16) + extra(2)。USB はこの構造体をそのまま送るため、ディスクリプタの宣言と一致していなければならない。
    CHECK(sizeof(keyboard_bitmap_report_t) == 1 + KEY_BITMAP_BYTES + 2);
    CHECK(KEY_EXTRA_KEYS == 2);
    CHECK(KC_KANJI1 == 0x87);
    CHECK(KC_KANJI3 == 0x89);
}

static void test_bitmap_keys(void) {
    keyboard_bitmap_report_t r;
    hid_report_clear(&r);

    hid_report_set_key(&r, KC_A, true);
    CHECK(hid_report_get_key(&r, KC_A));
    CHECK(!hid_report_get_key(&r, KC_B));
    CHECK(r.extra[0] == 0 && r.extra[1] == 0);

    hid_report_set_key(&r, KC_A, false);
    CHECK(!hid_report_get_key(&r, KC_A));

    // 境界: 0x7F は bitmap の最後のビット
    hid_report_set_key(&r, KC_MUTE, true);
    CHECK(hid_report_get_key(&r, KC_MUTE));
    CHECK(r.bitmap[KEY_BITMAP_BYTES - 1] == 0x80);
    CHECK(r.extra[0] == 0);
}

static void test_extra_keys(void) {
    keyboard_bitmap_report_t r;
    hid_report_clear(&r);

    // KANJI1 (0x87) は bitmap 範囲外なので配列へ入る
    hid_report_set_key(&r, KC_KANJI1, true);
    CHECK(hid_report_get_key(&r, KC_KANJI1));
    CHECK(r.extra[0] == KC_KANJI1);
    CHECK(r.extra[1] == 0);
    for (uint8_t i = 0; i < KEY_BITMAP_BYTES; ++i) {
        CHECK(r.bitmap[i] == 0);
    }

    hid_report_set_key(&r, KC_KANJI3, true);
    CHECK(hid_report_get_key(&r, KC_KANJI3));
    CHECK(r.extra[1] == KC_KANJI3);

    // 押しっぱなしの再セットでスロットを二重消費しない
    hid_report_set_key(&r, KC_KANJI1, true);
    CHECK(r.extra[0] == KC_KANJI1);
    CHECK(r.extra[1] == KC_KANJI3);

    hid_report_set_key(&r, KC_KANJI1, false);
    CHECK(!hid_report_get_key(&r, KC_KANJI1));
    CHECK(hid_report_get_key(&r, KC_KANJI3));
    CHECK(r.extra[0] == 0);

    hid_report_set_key(&r, KC_LANG1, true);
    CHECK(r.extra[0] == KC_LANG1);
}

static void test_extra_overflow(void) {
    keyboard_bitmap_report_t r;
    hid_report_clear(&r);

    hid_report_set_key(&r, KC_KANJI1, true);
    hid_report_set_key(&r, KC_KANJI3, true);
    hid_report_set_key(&r, KC_LANG1, true);
    CHECK(r.extra[0] == KC_KANJI1);
    CHECK(r.extra[1] == KC_KANJI3);
    CHECK(!hid_report_get_key(&r, KC_LANG1));

    hid_report_set_key(&r, KC_LANG2, false);
    CHECK(r.extra[0] == KC_KANJI1);
    CHECK(r.extra[1] == KC_KANJI3);
}

static void test_clear_and_equal(void) {
    keyboard_bitmap_report_t a, b;
    hid_report_clear(&a);
    hid_report_clear(&b);
    CHECK(hid_report_equal(&a, &b));

    // 配列側だけの違いも検出できないと再送が起きない
    hid_report_set_key(&a, KC_KANJI1, true);
    CHECK(!hid_report_equal(&a, &b));
    hid_report_set_key(&b, KC_KANJI1, true);
    CHECK(hid_report_equal(&a, &b));

    hid_report_clear(&a);
    CHECK(a.extra[0] == 0 && a.extra[1] == 0);
    CHECK(!hid_report_equal(&a, &b));
}

static void test_boot_report(void) {
    keyboard_bitmap_report_t r;
    keyboard_boot_report_t boot;
    hid_report_clear(&r);

    r.modifier = KC_MOD_BIT(KC_LSFT);
    hid_report_set_key(&r, KC_A, true);
    hid_report_set_key(&r, KC_KANJI3, true);

    hid_report_to_boot(&r, &boot);
    CHECK(boot.modifier == KC_MOD_BIT(KC_LSFT));
    CHECK(boot.reserved == 0);
    // bitmap 側が先、続けて上位 usage。ブートレポートは 8bit なので入る。
    CHECK(boot.keycodes[0] == KC_A);
    CHECK(boot.keycodes[1] == KC_KANJI3);
    CHECK(boot.keycodes[2] == 0);
}

static void test_boot_report_full(void) {
    keyboard_bitmap_report_t r;
    keyboard_boot_report_t boot;
    hid_report_clear(&r);

    // bitmap 側で 6 キー埋まったら上位 usage は入らない（溢れても壊れない）
    const uint8_t keys[6] = {KC_A, KC_B, KC_C, KC_D, KC_E, KC_F};
    for (int i = 0; i < 6; ++i) {
        hid_report_set_key(&r, keys[i], true);
    }
    hid_report_set_key(&r, KC_KANJI1, true);

    hid_report_to_boot(&r, &boot);
    for (int i = 0; i < 6; ++i) {
        CHECK(boot.keycodes[i] == keys[i]);
    }
}

int main(void) {
    test_layout();
    test_bitmap_keys();
    test_extra_keys();
    test_extra_overflow();
    test_clear_and_equal();
    test_boot_report();
    test_boot_report_full();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
