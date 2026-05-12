cmake_minimum_required(VERSION 3.13)
project(modbus_slave C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WHEELS ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(modbus_slave
    main.c
    ${WHEELS}/primitives/ring_buffer/ring_buffer.c
    ${WHEELS}/primitives/crc16/crc16.c
    ${WHEELS}/drivers/bmp280/bmp280.c
    ${WHEELS}/protocols/modbus_rtu/modbus_rtu.c
)

target_include_directories(modbus_slave PRIVATE
    ${WHEELS}/primitives/ring_buffer
    ${WHEELS}/primitives/crc16
    ${WHEELS}/drivers/bmp280
    ${WHEELS}/protocols/modbus_rtu
    ${TOOLING}/harness
)

target_compile_options(modbus_slave PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
