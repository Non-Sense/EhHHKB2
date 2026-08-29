# ホスト単体テスト。ファームウェア本体のビルドとは独立した CMake プロジェクト。
#
#   cmake -S tests -B build-tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure
#
# 対象はハードウェアに触れないロジックのみ（ui/config_menu, hid/hid_report）。

cmake_minimum_required(VERSION 3.13)
project(ehhhkb2_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# hid/keycode.h は HID_KEY_* を得るため TinyUSB の class/hid/hid.h を include する。
# ヘッダを読むだけなので、tusb_stub/tusb_config.h（最小構成）で足りる。
if (DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_ROOT $ENV{PICO_SDK_PATH})
else ()
    if (WIN32)
        set(USERHOME $ENV{USERPROFILE})
    else ()
        set(USERHOME $ENV{HOME})
    endif ()
    set(PICO_SDK_ROOT ${USERHOME}/.pico-sdk/sdk/2.2.0)
endif ()

set(TINYUSB_SRC ${PICO_SDK_ROOT}/lib/tinyusb/src)
if (NOT EXISTS ${TINYUSB_SRC}/class/hid/hid.h)
    message(FATAL_ERROR
            "TinyUSB のヘッダが見つかりません: ${TINYUSB_SRC}\n"
            "PICO_SDK_PATH を設定してください。")
endif ()

set(FW_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

add_executable(test_config_menu
        test_config_menu.c
        ${FW_ROOT}/ui/config_menu.c
        ${FW_ROOT}/hid/hid_report.c
)

add_executable(test_hid_report
        test_hid_report.c
        ${FW_ROOT}/hid/hid_report.c
)

foreach (target test_config_menu test_hid_report)
    target_include_directories(${target} PRIVATE
            ${FW_ROOT}
            ${CMAKE_CURRENT_LIST_DIR}/tusb_stub
            ${TINYUSB_SRC}
    )

    target_compile_options(${target} PRIVATE -Wall -Wextra)

    # TinyUSB の tusb_mcu.h は MCU 未指定だと #warning を出す。ヘッダを読むだけで
    # デバイススタックは使わないため、既定値を先回りして与えて黙らせる。
    # （tusb_mcu.h は tusb_config.h より先に読まれるのでコマンドラインで定義する）
    target_compile_definitions(${target} PRIVATE TUP_DCD_ENDPOINT_MAX=8)
endforeach ()

enable_testing()
add_test(NAME config_menu COMMAND test_config_menu)
add_test(NAME hid_report COMMAND test_hid_report)
