cmake_minimum_required(VERSION 3.13)
project(test_ring_log C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ring_log
    test_ring_log.c
    ../ring_log.c
)

target_include_directories(test_ring_log PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_ring_log PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ring_log_tests COMMAND test_ring_log)
