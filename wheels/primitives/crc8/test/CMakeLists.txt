cmake_minimum_required(VERSION 3.13)
project(test_crc8 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_crc8
    test_crc8.c
    ../crc8.c
)

target_include_directories(test_crc8 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_crc8 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_crc8
    test_null_crc8.c
    ../crc8.c
)
target_include_directories(test_null_crc8 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_crc8 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME crc8_tests COMMAND test_crc8)
add_test(NAME crc8_null_tests COMMAND test_null_crc8)
