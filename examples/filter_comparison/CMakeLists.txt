cmake_minimum_required(VERSION 3.13)
project(filter_comparison C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WHEELS ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(filter_comparison
    main.c
    ${WHEELS}/primitives/median_filter/median_filter.c
    ${WHEELS}/primitives/iir_lowpass/iir_lowpass.c
    ${WHEELS}/primitives/kalman_1d/kalman_1d.c
)

target_include_directories(filter_comparison PRIVATE
    ${WHEELS}/primitives/median_filter
    ${WHEELS}/primitives/iir_lowpass
    ${WHEELS}/primitives/kalman_1d
    ${TOOLING}/harness
)

target_compile_options(filter_comparison PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

target_link_libraries(filter_comparison PRIVATE m)
