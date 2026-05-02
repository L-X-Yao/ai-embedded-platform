cmake_minimum_required(VERSION 3.13)
project(test_moving_window C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_moving_window
    test_moving_window.c
    ../moving_window.c
)

target_include_directories(test_moving_window PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_moving_window PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME moving_window_tests COMMAND test_moving_window)
