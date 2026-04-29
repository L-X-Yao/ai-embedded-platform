cmake_minimum_required(VERSION 3.13)
project(test_debounce C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_debounce test_debounce.c ../debounce.c)
target_include_directories(test_debounce PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(test_debounce PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME debounce_tests COMMAND test_debounce)
