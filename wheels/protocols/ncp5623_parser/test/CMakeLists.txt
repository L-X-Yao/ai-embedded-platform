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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_compile_options(test_ncp5623_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_ncp5623_parser
    test_null_ncp5623_parser.c
    ../ncp5623_parser.c
)
target_include_directories(test_null_ncp5623_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_ncp5623_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ncp5623_parser_tests COMMAND test_ncp5623_parser)
add_test(NAME ncp5623_parser_null_tests COMMAND test_null_ncp5623_parser)
