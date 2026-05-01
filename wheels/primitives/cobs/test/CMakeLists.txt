cmake_minimum_required(VERSION 3.13)
project(test_cobs C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_cobs
    test_cobs.c
    ../cobs.c
)

target_include_directories(test_cobs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_cobs PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME cobs_tests COMMAND test_cobs)
