cmake_minimum_required(VERSION 3.13)
project(test_log_rtt C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_log_rtt
    test_log_rtt.c
    ../log_rtt.c
)

target_include_directories(test_log_rtt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_log_rtt PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME log_rtt_tests COMMAND test_log_rtt)
