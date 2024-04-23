cmake_minimum_required(VERSION 3.12)

message("Build type: \"${CMAKE_BUILD_TYPE}\"")

# Project name
set(NAME tusb_gamepad_ds4_to_xinput_pico-sdk)

set(PICO_BOARD none)
add_compile_definitions(PICO_BOOT_STAGE2_CHOOSE_GENERIC_03H=1 PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

set(ROOT ${CMAKE_CURRENT_LIST_DIR})
set(PICO_PIO_USB_PATH ${ROOT}/lib/Pico-PIO-USB)
set(PICO_TINYUSB_PATH ${ROOT}/lib/tinyusb)
set(TUSB_GAMEPAD_PATH ${ROOT}/lib/tusb_gamepad)

pico_sdk_init()

add_subdirectory(${PICO_PIO_USB_PATH})
add_subdirectory(${TUSB_GAMEPAD_PATH})
target_include_directories(tusb_gamepad PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src) # so tusb_gamepad can see tusb_config.h

file(GLOB_RECURSE SOURCES 
    "src/main.c"
    "src/hid_app.c")

add_executable(${NAME} ${SOURCES})

target_include_directories(${NAME} PUBLIC
    ${ROOT}/src 
    ${ROOT}/lib)

target_link_libraries(${NAME}
    pico_stdlib
    pico_multicore
    hardware_pio
    tinyusb_board
    tinyusb_host
    pico_pio_usb
    tusb_gamepad)

pico_add_extra_outputs(${NAME})