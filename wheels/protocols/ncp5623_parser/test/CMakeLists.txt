cmake_minimum_required(VERSION 3.13)
project(test_ncp5623_parser C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ncp5623_parser
    test_ncp5623_parser.c
    ../ncp5623_parser.c
)

target_include_directories(test_ncp5623_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

enable_testing()
add_test(NAME ncp5623_parser_tests COMMAND test_ncp5623_parser)
