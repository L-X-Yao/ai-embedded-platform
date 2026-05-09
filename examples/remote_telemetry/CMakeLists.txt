cmake_minimum_required(VERSION 3.13)
project(remote_telemetry C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- Wheel source paths (relative to repo root) ----------------------
set(WHEELS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../wheels")
set(TOOLING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../tooling")

add_executable(remote_telemetry
    telemetry.c

    # L0 wheels
    ${WHEELS_DIR}/primitives/ring_buffer/ring_buffer.c
    ${WHEELS_DIR}/primitives/crc16/crc16.c
    ${WHEELS_DIR}/primitives/json_parser/json_parser.c

    # L2 wheels
    ${WHEELS_DIR}/protocols/mqtt_packet/mqtt_packet.c
    ${WHEELS_DIR}/protocols/at_command/at_command.c
)

target_include_directories(remote_telemetry PRIVATE
    # L0 wheel headers
    ${WHEELS_DIR}/primitives/ring_buffer
    ${WHEELS_DIR}/primitives/crc16
    ${WHEELS_DIR}/primitives/json_parser

    # L2 wheel headers
    ${WHEELS_DIR}/protocols/mqtt_packet
    ${WHEELS_DIR}/protocols/at_command

    # contract.h (used internally by wheels)
    ${TOOLING_DIR}/harness
)

target_compile_options(remote_telemetry PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
