cmake_minimum_required(VERSION 3.13)
project(test_modbus_rtu C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_modbus_rtu
    test_modbus_rtu.c
    ../modbus_rtu.c
    ../../../primitives/crc16/crc16.c
)

target_include_directories(test_modbus_rtu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../primitives/crc16
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_compile_options(test_modbus_rtu PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_modbus_rtu
    test_null_modbus_rtu.c
    ../modbus_rtu.c
    ../../../primitives/crc16/crc16.c
)
target_include_directories(test_null_modbus_rtu PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../primitives/crc16
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_modbus_rtu PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME modbus_rtu_tests COMMAND test_modbus_rtu)
add_test(NAME modbus_rtu_null_tests COMMAND test_null_modbus_rtu)
