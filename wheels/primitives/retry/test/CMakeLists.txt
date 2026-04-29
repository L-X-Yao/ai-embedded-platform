cmake_minimum_required(VERSION 3.13)
project(test_retry C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_retry
    test_retry.c
    ../retry.c
)
target_include_directories(test_retry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(test_retry PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME retry_tests COMMAND test_retry)
