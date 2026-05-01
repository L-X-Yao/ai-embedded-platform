cmake_minimum_required(VERSION 3.13)
project(test_syslog_packet C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_syslog_packet
    test_syslog_packet.c
    ../syslog_packet.c
)

target_include_directories(test_syslog_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_syslog_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME syslog_packet_tests COMMAND test_syslog_packet)
