cmake_minimum_required(VERSION 3.13)
project(mqtt_telemetry_demo C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(DRV       ${WHEELS}/drivers)
set(PROTO     ${WHEELS}/protocols)
set(MOD       ${WHEELS}/modules)

add_executable(mqtt_telemetry_demo
    mqtt_telemetry_demo.c

    # L1 driver
    ${DRV}/bmp280/bmp280.c

    # L2 protocol
    ${PROTO}/mqtt_packet/mqtt_packet.c

    # L3 modules
    ${MOD}/sensor_hub/sensor_hub.c
    ${MOD}/mqtt_client/mqtt_client.c
)

target_include_directories(mqtt_telemetry_demo PRIVATE
    ${DRV}/bmp280
    ${PROTO}/mqtt_packet
    ${MOD}/sensor_hub
    ${MOD}/mqtt_client

    # contract.h
    ${TOOLING}/harness
)

target_compile_options(mqtt_telemetry_demo PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME mqtt_telemetry_demo_integration COMMAND mqtt_telemetry_demo)
