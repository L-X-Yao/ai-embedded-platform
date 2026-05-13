cmake_minimum_required(VERSION 3.13)
project(test_dns_packet C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_dns_packet
    test_dns_packet.c
    ../dns_packet.c
)

target_include_directories(test_dns_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_dns_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_dns_packet
    test_null_dns_packet.c
    ../dns_packet.c
)
target_include_directories(test_null_dns_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_dns_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_dns_packet
    test_boundary_dns_packet.c
    ../dns_packet.c
)
target_include_directories(test_boundary_dns_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_dns_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME dns_packet_tests COMMAND test_dns_packet)
add_test(NAME dns_packet_null_tests COMMAND test_null_dns_packet)
add_test(NAME dns_packet_boundary_tests COMMAND test_boundary_dns_packet)
