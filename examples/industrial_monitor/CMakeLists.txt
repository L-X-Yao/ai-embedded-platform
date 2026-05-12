cmake_minimum_required(VERSION 3.13)
project(industrial_monitor C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(DRV       ${WHEELS}/drivers)
set(PROTO     ${WHEELS}/protocols)
set(MOD       ${WHEELS}/modules)

# ----- executable: industrial_monitor -----
add_executable(industrial_monitor
    industrial_monitor.c

    # L0 - pid_controller
    ${PRIM}/pid_controller/pid_controller.c

    # L0 - rate_limiter
    ${PRIM}/rate_limiter/rate_limiter.c

    # L0 - fixed_point
    ${PRIM}/fixed_point/fixed_point.c

    # L1 - ina219
    ${DRV}/ina219/ina219.c

    # L1 - ads1115
    ${DRV}/ads1115/ads1115.c

    # L2 - canbus
    ${PROTO}/canbus/canbus.c

    # L3 - telemetry
    ${MOD}/telemetry/telemetry.c

    # L3 - task_watchdog
    ${MOD}/task_watchdog/task_watchdog.c
)

target_include_directories(industrial_monitor PRIVATE
    # wheel headers
    ${PRIM}/pid_controller
    ${PRIM}/rate_limiter
    ${PRIM}/fixed_point
    ${DRV}/ina219
    ${DRV}/ads1115
    ${PROTO}/canbus
    ${MOD}/telemetry
    ${MOD}/task_watchdog

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(industrial_monitor PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME industrial_monitor_integration COMMAND industrial_monitor)
