cmake_minimum_required(VERSION 3.13)
project(command_queue_demo C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(MOD       ${WHEELS}/modules)

add_executable(command_queue_demo
    command_queue_demo.c

    # L0 primitives
    ${PRIM}/ring_buffer/ring_buffer.c
    ${PRIM}/cli_parser/cli_parser.c
    ${PRIM}/string_utils/string_utils.c

    # L3 module
    ${MOD}/command_queue/command_queue.c
)

target_include_directories(command_queue_demo PRIVATE
    ${PRIM}/ring_buffer
    ${PRIM}/cli_parser
    ${PRIM}/string_utils
    ${MOD}/command_queue

    # contract.h
    ${TOOLING}/harness
)

target_compile_options(command_queue_demo PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME command_queue_demo_integration COMMAND command_queue_demo)
