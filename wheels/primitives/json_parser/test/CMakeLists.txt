cmake_minimum_required(VERSION 3.13)
project(test_json_parser C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_json_parser
    test_json_parser.c
    ../json_parser.c
)

target_include_directories(test_json_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_json_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_json_parser
    test_null_json_parser.c
    ../json_parser.c
)
target_include_directories(test_null_json_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_json_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME json_parser_tests COMMAND test_json_parser)
add_test(NAME json_parser_null_tests COMMAND test_null_json_parser)
