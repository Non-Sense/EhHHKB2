# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if (WIN32)
    set(USERHOME $ENV{USERPROFILE})
else ()
    set(USERHOME $ENV{HOME})
endif ()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif ()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ehhhkb2 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(ehhhkb2
        ehhhkb2.c
        hid/hid_report.c
        hid/key_matrix.c
        hid/output_router.c
        usb/usb_hid.c
        ble/ble_hid.c
        usb/usb_descriptors.c
        display/ssd1306.c
        power/power_monitor.c
        ui/config_menu.c
        ui/display_policy.c
        ui/screen.c
)

pico_set_program_name(ehhhkb2 "ehhhkb2")
pico_set_program_version(ehhhkb2 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(ehhhkb2 0)
pico_enable_stdio_usb(ehhhkb2 0)

# Add the standard library to the build
target_link_libraries(ehhhkb2
        pico_stdlib
        pico_unique_id
        pico_multicore
        pico_lwip_nosys
        tinyusb_device
        tinyusb_board
        pico_btstack_ble
        pico_btstack_cyw43
        pico_cyw43_arch_poll
        hardware_adc
        hardware_i2c)

# Add the standard include files to the build
target_include_directories(ehhhkb2 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/ble
        ${CMAKE_CURRENT_LIST_DIR}/hid
        ${CMAKE_CURRENT_LIST_DIR}/usb
        ${CMAKE_CURRENT_LIST_DIR}/display
        ${CMAKE_CURRENT_LIST_DIR}/power
        ${CMAKE_CURRENT_LIST_DIR}/ui
)

pico_btstack_make_gatt_header(
        ehhhkb2
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/ble/ehhhkb2.gatt
        ${PICO_SDK_PATH}/lib/btstack/src/ble/gatt-service
)

pico_add_extra_outputs(ehhhkb2)
