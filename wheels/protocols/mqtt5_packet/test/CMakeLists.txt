cmake_minimum_required(VERSION 3.13)
project(test_mqtt5_packet C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mqtt5_packet
    test_mqtt5_packet.c
    ../mqtt5_packet.c
)

target_include_directories(test_mqtt5_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mqtt5_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME mqtt5_packet_tests COMMAND test_mqtt5_packet)
