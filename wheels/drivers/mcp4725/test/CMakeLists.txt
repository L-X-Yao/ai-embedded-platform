cmake_minimum_required(VERSION 3.13)
project(test_mcp4725 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mcp4725
    test_mcp4725.c
    ../mcp4725.c
)

target_include_directories(test_mcp4725 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mcp4725 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME mcp4725_tests COMMAND test_mcp4725)
