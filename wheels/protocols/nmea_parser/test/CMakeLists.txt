cmake_minimum_required(VERSION 3.13)
project(test_nmea_parser C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_nmea_parser
    test_nmea_parser.c
    ../nmea_parser.c
)

target_include_directories(test_nmea_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_nmea_parser PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME nmea_parser_tests COMMAND test_nmea_parser)
