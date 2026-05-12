cmake_minimum_required(VERSION 3.13)
project(timer_retry C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WHEELS ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(timer_retry
    main.c
    ${WHEELS}/primitives/soft_timer/soft_timer.c
    ${WHEELS}/primitives/retry/retry.c
)

target_include_directories(timer_retry PRIVATE
    ${WHEELS}/primitives/soft_timer
    ${WHEELS}/primitives/retry
    ${TOOLING}/harness
)

target_compile_options(timer_retry PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
