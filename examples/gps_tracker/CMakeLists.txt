cmake_minimum_required(VERSION 3.13)
project(gps_tracker C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(DRV       ${WHEELS}/drivers)
set(PROTO     ${WHEELS}/protocols)

# ----- executable: gps_tracker -----
add_executable(gps_tracker
    gps_tracker.c

    # L0 - kalman_1d
    ${PRIM}/kalman_1d/kalman_1d.c

    # L0 - base64
    ${PRIM}/base64/base64.c

    # L0 - uuid
    ${PRIM}/uuid/uuid.c

    # L1 - ds3231
    ${DRV}/ds3231/ds3231.c

    # L1 - sd_spi
    ${DRV}/sd_spi/sd_spi.c

    # L2 - sntp_client
    ${PROTO}/sntp_client/sntp_client.c

    # L2 - ubx_parser
    ${PROTO}/ubx_parser/ubx_parser.c
)

target_include_directories(gps_tracker PRIVATE
    # wheel headers
    ${PRIM}/kalman_1d
    ${PRIM}/base64
    ${PRIM}/uuid
    ${DRV}/ds3231
    ${DRV}/sd_spi
    ${PROTO}/sntp_client
    ${PROTO}/ubx_parser

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(gps_tracker PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME gps_tracker_integration COMMAND gps_tracker)
