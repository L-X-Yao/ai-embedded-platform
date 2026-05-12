cmake_minimum_required(VERSION 3.13)
project(wireless_mesh C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(DRV       ${WHEELS}/drivers)
set(PROTO     ${WHEELS}/protocols)

# ----- executable: wireless_mesh -----
add_executable(wireless_mesh
    wireless_mesh.c

    # L0 - slip
    ${PRIM}/slip/slip.c

    # L0 - checksum
    ${PRIM}/checksum/checksum.c

    # L0 - rle_codec
    ${PRIM}/rle_codec/rle_codec.c

    # L1 - nrf24l01
    ${DRV}/nrf24l01/nrf24l01.c

    # L1 - lora_sx1276
    ${DRV}/lora_sx1276/lora_sx1276.c

    # L2 - lorawan_packet
    ${PROTO}/lorawan_packet/lorawan_packet.c
)

target_include_directories(wireless_mesh PRIVATE
    # wheel headers
    ${PRIM}/slip
    ${PRIM}/checksum
    ${PRIM}/rle_codec
    ${DRV}/nrf24l01
    ${DRV}/lora_sx1276
    ${PROTO}/lorawan_packet

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(wireless_mesh PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME wireless_mesh_integration COMMAND wireless_mesh)
