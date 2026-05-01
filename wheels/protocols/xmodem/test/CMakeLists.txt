cmake_minimum_required(VERSION 3.13)
project(test_xmodem C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_xmodem
    test_xmodem.c
    ../xmodem.c
)

target_include_directories(test_xmodem PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_xmodem PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME xmodem_tests COMMAND test_xmodem)
