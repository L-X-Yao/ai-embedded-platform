cmake_minimum_required(VERSION 3.13)
project(test_modbus_rtu C)

add_executable(test_modbus_rtu
    test_modbus_rtu.c
    ../modbus_rtu.c
    ../../../primitives/crc16/crc16.c
)

target_include_directories(test_modbus_rtu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../primitives/crc16
)

enable_testing()
add_test(NAME modbus_rtu_tests COMMAND test_modbus_rtu)
