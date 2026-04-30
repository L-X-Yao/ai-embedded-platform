cmake_minimum_required(VERSION 3.13)
project(test_crc16 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_crc16
    test_crc16.c
    ../crc16.c
)
target_include_directories(test_crc16 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_crc16 PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME crc16_tests COMMAND test_crc16)
