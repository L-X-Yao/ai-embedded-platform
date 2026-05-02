cmake_minimum_required(VERSION 3.13)
project(test_cbor_lite C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_cbor_lite
    test_cbor_lite.c
    ../cbor_lite.c
)

target_include_directories(test_cbor_lite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_cbor_lite PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME cbor_lite_tests COMMAND test_cbor_lite)
