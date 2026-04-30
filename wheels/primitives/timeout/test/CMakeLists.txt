cmake_minimum_required(VERSION 3.13)
project(test_timeout C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_timeout
    test_timeout.c
    ../timeout.c
)
target_include_directories(test_timeout PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_timeout PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME timeout_tests COMMAND test_timeout)
