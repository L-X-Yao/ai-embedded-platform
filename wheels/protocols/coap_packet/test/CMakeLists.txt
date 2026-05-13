cmake_minimum_required(VERSION 3.13)
project(test_coap_packet C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_coap_packet
    test_coap_packet.c
    ../coap_packet.c
)
target_include_directories(test_coap_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_coap_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_coap_packet
    test_null_coap_packet.c
    ../coap_packet.c
)
target_include_directories(test_null_coap_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_coap_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_coap_packet
    test_boundary_coap_packet.c
    ../coap_packet.c
)
target_include_directories(test_boundary_coap_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_coap_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME coap_packet_tests COMMAND test_coap_packet)
add_test(NAME coap_packet_null_tests COMMAND test_null_coap_packet)
add_test(NAME coap_packet_boundary_tests COMMAND test_boundary_coap_packet)
