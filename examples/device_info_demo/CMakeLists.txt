cmake_minimum_required(VERSION 3.13)
project(device_info_demo C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(MOD       ${WHEELS}/modules)

add_executable(device_info_demo
    device_info_demo.c

    # L0 primitives
    ${PRIM}/log/log.c
    ${PRIM}/uuid/uuid.c

    # L3 modules
    ${MOD}/version_info/version_info.c
    ${MOD}/device_info/device_info.c
)

target_include_directories(device_info_demo PRIVATE
    ${PRIM}/log
    ${PRIM}/uuid
    ${MOD}/version_info
    ${MOD}/device_info

    # contract.h
    ${TOOLING}/harness
)

target_compile_options(device_info_demo PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME device_info_demo_integration COMMAND device_info_demo)
