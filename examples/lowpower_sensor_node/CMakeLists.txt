cmake_minimum_required(VERSION 3.13)
project(lowpower_sensor_node C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(DRV       ${WHEELS}/drivers)
set(MOD       ${WHEELS}/modules)

# ----- executable: lowpower_sensor_node -----
add_executable(lowpower_sensor_node
    lowpower_sensor_node.c

    # L1 - max17048 (battery fuel gauge)
    ${DRV}/max17048/max17048.c

    # L1 - ds3231 (RTC)
    ${DRV}/ds3231/ds3231.c

    # L1 - adxl345 (accelerometer / motion wake)
    ${DRV}/adxl345/adxl345.c

    # L1 - bme280 (env sensor)
    ${DRV}/bme280/bme280.c

    # L3 - boot_counter (persistent wake count)
    ${MOD}/boot_counter/boot_counter.c

    # L3 - power_manager (sleep level orchestration)
    ${MOD}/power_manager/power_manager.c
)

target_include_directories(lowpower_sensor_node PRIVATE
    # wheel headers
    ${DRV}/max17048
    ${DRV}/ds3231
    ${DRV}/adxl345
    ${DRV}/bme280
    ${MOD}/boot_counter
    ${MOD}/power_manager

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(lowpower_sensor_node PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME lowpower_sensor_node_integration COMMAND lowpower_sensor_node)
