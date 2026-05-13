cmake_minimum_required(VERSION 3.13)
project(can_gateway C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(DRV       ${WHEELS}/drivers)
set(PROTO     ${WHEELS}/protocols)

# ----- executable: can_gateway -----
add_executable(can_gateway
    can_gateway.c

    # L1 - mcp2515 (SPI CAN controller)
    ${DRV}/mcp2515/mcp2515.c

    # L2 - canbus (generic frame pack / unpack / signal extract)
    ${PROTO}/canbus/canbus.c

    # L2 - isotp (ISO-TP reassembly)
    ${PROTO}/isotp/isotp.c

    # L2 - j1939 (PGN / priority / source decode)
    ${PROTO}/j1939/j1939.c
)

target_include_directories(can_gateway PRIVATE
    # wheel headers
    ${DRV}/mcp2515
    ${PROTO}/canbus
    ${PROTO}/isotp
    ${PROTO}/j1939

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(can_gateway PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME can_gateway_integration COMMAND can_gateway)
