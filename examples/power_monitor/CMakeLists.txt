cmake_minimum_required(VERSION 3.13)
project(power_monitor C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(DRV       ${WHEELS}/drivers)
set(PROTO     ${WHEELS}/protocols)
set(MOD       ${WHEELS}/modules)

# ----- executable: power_monitor -----
add_executable(power_monitor
    power_monitor.c

    # L0 - bitmap
    ${PRIM}/bitmap/bitmap.c

    # L0 - memory_pool
    ${PRIM}/memory_pool/memory_pool.c

    # L0 - timeout
    ${PRIM}/timeout/timeout.c

    # L0 - endian
    ${PRIM}/endian/endian.c

    # L1 - max17048
    ${DRV}/max17048/max17048.c

    # L1 - ina226
    ${DRV}/ina226/ina226.c

    # L1 - gpio_button
    ${DRV}/gpio_button/gpio_button.c

    # L2 - i2c_scan
    ${PROTO}/i2c_scan/i2c_scan.c

    # L3 - power_manager
    ${MOD}/power_manager/power_manager.c

    # L3 - button_handler
    ${MOD}/button_handler/button_handler.c

    # L3 - ring_log
    ${MOD}/ring_log/ring_log.c
)

target_include_directories(power_monitor PRIVATE
    # wheel headers
    ${PRIM}/bitmap
    ${PRIM}/memory_pool
    ${PRIM}/timeout
    ${PRIM}/endian
    ${DRV}/max17048
    ${DRV}/ina226
    ${DRV}/gpio_button
    ${PROTO}/i2c_scan
    ${MOD}/power_manager
    ${MOD}/button_handler
    ${MOD}/ring_log

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(power_monitor PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME power_monitor_integration COMMAND power_monitor)
