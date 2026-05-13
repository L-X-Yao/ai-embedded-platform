cmake_minimum_required(VERSION 3.13)
project(test_ubx_parser C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ubx_parser
    test_ubx_parser.c
    ../ubx_parser.c
)

target_include_directories(test_ubx_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ubx_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_ubx_parser
    test_null_ubx_parser.c
    ../ubx_parser.c
)
target_include_directories(test_null_ubx_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ubx_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_boundary_ubx_parser
    test_boundary_ubx_parser.c
    ../ubx_parser.c
)
target_include_directories(test_boundary_ubx_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_ubx_parser PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ubx_parser_tests COMMAND test_ubx_parser)
add_test(NAME ubx_parser_null_tests COMMAND test_null_ubx_parser)
add_test(NAME ubx_parser_boundary_tests COMMAND test_boundary_ubx_parser)
