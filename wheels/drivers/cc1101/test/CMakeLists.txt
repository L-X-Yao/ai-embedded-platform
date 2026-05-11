cmake_minimum_required(VERSION 3.13)
project(test_cc1101 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_cc1101
    test_cc1101.c
    ../cc1101.c
)

target_include_directories(test_cc1101 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_cc1101 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME cc1101_tests COMMAND test_cc1101)

add_executable(test_null_cc1101
    test_null_cc1101.c
    ../cc1101.c
)
target_include_directories(test_null_cc1101 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_cc1101 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME cc1101_null_tests COMMAND test_null_cc1101)
