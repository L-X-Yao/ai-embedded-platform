cmake_minimum_required(VERSION 3.13)
project(test_a4988 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_a4988
    test_a4988.c
    ../a4988.c
)

target_include_directories(test_a4988 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_a4988 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME a4988_tests COMMAND test_a4988)

add_executable(test_null_a4988
    test_null_a4988.c
    ../a4988.c
)
target_include_directories(test_null_a4988 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_a4988 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME a4988_null_tests COMMAND test_null_a4988)