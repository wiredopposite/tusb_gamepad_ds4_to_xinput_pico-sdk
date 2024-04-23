cmake_minimum_required(VERSION 3.12)

message("Build type: \"${CMAKE_BUILD_TYPE}\"")

# Project name
set(NAME tusb_gamepad_example_pico-sdk)

# Board type
set(PICO_BOARD none)

# Fixes that allow some MCH2022 badges with a slowly starting oscillator to boot properly
add_compile_definitions(PICO_BOOT_STAGE2_CHOOSE_GENERIC_03H=1 PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

# SDK
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)
set(TUSB_GAMEPAD_ROOT ${CMAKE_CURRENT_LIST_DIR}/lib/tusb_gamepad)

add_subdirectory(${TUSB_GAMEPAD_ROOT})
target_include_directories(tusb_gamepad PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src) # so tusb_gamepad can see your tusb_config.h file

pico_sdk_init()

file(GLOB_RECURSE SOURCES "src/main.c")
add_executable(${NAME} ${SOURCES})

target_include_directories(${NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/lib)

include_directories(${CMAKE_CURRENT_LIST_DIR}/lib)

target_link_libraries(${NAME}
    pico_stdlib
    tinyusb_device
    tinyusb_board
    tusb_gamepad
)

pico_add_extra_outputs(${NAME})
