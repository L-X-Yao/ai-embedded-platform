cmake_minimum_required(VERSION 3.13)
project(test_ymodem C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ymodem
    test_ymodem.c
    ../ymodem.c
)

target_include_directories(test_ymodem PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_ymodem PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ymodem_tests COMMAND test_ymodem)
