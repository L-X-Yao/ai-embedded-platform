cmake_minimum_required(VERSION 3.13)
project(test_log_color C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_log_color
    test_log_color.c
    ../log_color.c
)

target_include_directories(test_log_color PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_log_color PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME log_color_tests COMMAND test_log_color)
