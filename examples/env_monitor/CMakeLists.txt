cmake_minimum_required(VERSION 3.13)
project(env_monitor C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(DRV       ${WHEELS}/drivers)
set(MOD       ${WHEELS}/modules)

# ----- executable: env_monitor -----
add_executable(env_monitor
    env_monitor.c

    # L0 - ring_buffer
    ${PRIM}/ring_buffer/ring_buffer.c

    # L1 - bmp280
    ${DRV}/bmp280/bmp280.c

    # L1 - sht30
    ${DRV}/sht30/sht30.c

    # L3 - sensor_hub
    ${MOD}/sensor_hub/sensor_hub.c

    # L3 - data_logger
    ${MOD}/data_logger/data_logger.c
)

target_include_directories(env_monitor PRIVATE
    # wheel headers
    ${PRIM}/ring_buffer
    ${DRV}/bmp280
    ${DRV}/sht30
    ${MOD}/sensor_hub
    ${MOD}/data_logger

    # test_harness.h + contract.h
    ${TOOLING}/test
)

target_compile_options(env_monitor PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME env_monitor_integration COMMAND env_monitor)
