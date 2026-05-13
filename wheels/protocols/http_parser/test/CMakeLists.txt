cmake_minimum_required(VERSION 3.13)
project(test_http_parser C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_http_parser
    test_http_parser.c
    ../http_parser.c
)
target_include_directories(test_http_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_http_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_http_parser
    test_null_http_parser.c
    ../http_parser.c
)
target_include_directories(test_null_http_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_http_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_http_parser
    test_boundary_http_parser.c
    ../http_parser.c
)
target_include_directories(test_boundary_http_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_http_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME http_parser_tests COMMAND test_http_parser)
add_test(NAME http_parser_null_tests COMMAND test_null_http_parser)
add_test(NAME http_parser_boundary_tests COMMAND test_boundary_http_parser)
