cmake_minimum_required(VERSION 3.13)
project(motor_control C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WHEELS ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(motor_control
    main.c
    ${WHEELS}/drivers/a4988/a4988.c
    ${WHEELS}/primitives/cli_parser/cli_parser.c
    ${WHEELS}/primitives/state_machine/state_machine.c
)

target_include_directories(motor_control PRIVATE
    ${WHEELS}/drivers/a4988
    ${WHEELS}/primitives/cli_parser
    ${WHEELS}/primitives/state_machine
    ${TOOLING}/harness
)

target_compile_options(motor_control PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
