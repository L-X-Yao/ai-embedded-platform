cmake_minimum_required(VERSION 3.13)
project(test_cli_parser C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_cli_parser
    test_cli_parser.c
    ../cli_parser.c
)

target_include_directories(test_cli_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

enable_testing()
add_test(NAME cli_parser_tests COMMAND test_cli_parser)
