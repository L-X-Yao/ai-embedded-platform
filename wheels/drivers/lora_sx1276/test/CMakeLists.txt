cmake_minimum_required(VERSION 3.13)
project(test_lora_sx1276 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lora_sx1276
    test_lora_sx1276.c
    ../lora_sx1276.c
)

target_include_directories(test_lora_sx1276 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_lora_sx1276 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME lora_sx1276_tests COMMAND test_lora_sx1276)
