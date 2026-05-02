cmake_minimum_required(VERSION 3.13)
project(test_drv2605 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_drv2605
    test_drv2605.c
    ../drv2605.c
)

target_include_directories(test_drv2605 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_drv2605 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME drv2605_tests COMMAND test_drv2605)
