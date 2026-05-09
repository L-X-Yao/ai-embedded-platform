cmake_minimum_required(VERSION 3.13)
project(test_crc_any C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_crc_any
    test_crc_any.c
    ../crc_any.c
)

target_include_directories(test_crc_any PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_crc_any PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_crc_any
    test_null_crc_any.c
    ../crc_any.c
)
target_include_directories(test_null_crc_any PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_crc_any PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME crc_any_tests COMMAND test_crc_any)
add_test(NAME crc_any_null_tests COMMAND test_null_crc_any)
