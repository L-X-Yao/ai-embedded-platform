cmake_minimum_required(VERSION 3.13)
project(test_printf_tiny C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_printf_tiny
    test_printf_tiny.c
    ../printf_tiny.c
)

target_include_directories(test_printf_tiny PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_printf_tiny PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_printf_tiny
    test_null_printf_tiny.c
    ../printf_tiny.c
)
target_include_directories(test_null_printf_tiny PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_printf_tiny PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME printf_tiny_tests COMMAND test_printf_tiny)
add_test(NAME printf_tiny_null_tests COMMAND test_null_printf_tiny)
