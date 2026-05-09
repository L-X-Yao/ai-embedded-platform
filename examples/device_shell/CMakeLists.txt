cmake_minimum_required(VERSION 3.13)
project(device_shell C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------- Wheel source roots ----------
set(WHEELS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(device_shell
    device_shell.c
    ${WHEELS_DIR}/modules/cli_shell/cli_shell.c
    ${WHEELS_DIR}/modules/config_store/config_store.c
    ${WHEELS_DIR}/modules/param_table/param_table.c
    ${WHEELS_DIR}/modules/version_info/version_info.c
    ${WHEELS_DIR}/modules/error_handler/error_handler.c
)

target_include_directories(device_shell PRIVATE
    ${WHEELS_DIR}/modules/cli_shell
    ${WHEELS_DIR}/modules/config_store
    ${WHEELS_DIR}/modules/param_table
    ${WHEELS_DIR}/modules/version_info
    ${WHEELS_DIR}/modules/error_handler
    ${TOOLING_DIR}/harness            # contract.h
)

target_compile_options(device_shell PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
