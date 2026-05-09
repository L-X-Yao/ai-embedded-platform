cmake_minimum_required(VERSION 3.13)
project(test_dhcp_packet C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_dhcp_packet
    test_dhcp_packet.c
    ../dhcp_packet.c
)

target_include_directories(test_dhcp_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_dhcp_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_dhcp_packet
    test_null_dhcp_packet.c
    ../dhcp_packet.c
)
target_include_directories(test_null_dhcp_packet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_dhcp_packet PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME dhcp_packet_tests COMMAND test_dhcp_packet)
add_test(NAME dhcp_packet_null_tests COMMAND test_null_dhcp_packet)
