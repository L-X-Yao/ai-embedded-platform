cmake_minimum_required(VERSION 3.13)
project(test_rotary_encoder C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_rotary_encoder test_rotary_encoder.c ../rotary_encoder.c)
target_include_directories(test_rotary_encoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_rotary_encoder PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME rotary_encoder_tests COMMAND test_rotary_encoder)
