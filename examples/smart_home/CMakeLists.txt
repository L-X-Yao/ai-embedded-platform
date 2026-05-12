cmake_minimum_required(VERSION 3.13)
project(smart_home C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(DRV       ${WHEELS}/drivers)
set(PROTO     ${WHEELS}/protocols)
set(MOD       ${WHEELS}/modules)

# ----- executable: smart_home -----
add_executable(smart_home
    smart_home.c

    # L0 - debounce
    ${PRIM}/debounce/debounce.c

    # L0 - timer_wheel
    ${PRIM}/timer_wheel/timer_wheel.c

    # L0 - led_pattern
    ${PRIM}/led_pattern/led_pattern.c

    # L1 - bme280
    ${DRV}/bme280/bme280.c

    # L1 - bh1750
    ${DRV}/bh1750/bh1750.c

    # L1 - ws2812
    ${DRV}/ws2812/ws2812.c

    # L2 - cbor_lite
    ${PROTO}/cbor_lite/cbor_lite.c

    # L3 - scheduler
    ${MOD}/scheduler/scheduler.c

    # L3 - event_bus
    ${MOD}/event_bus/event_bus.c
)

target_include_directories(smart_home PRIVATE
    # wheel headers
    ${PRIM}/debounce
    ${PRIM}/timer_wheel
    ${PRIM}/led_pattern
    ${DRV}/bme280
    ${DRV}/bh1750
    ${DRV}/ws2812
    ${PROTO}/cbor_lite
    ${MOD}/scheduler
    ${MOD}/event_bus

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(smart_home PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME smart_home_integration COMMAND smart_home)
