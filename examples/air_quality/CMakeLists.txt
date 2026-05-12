cmake_minimum_required(VERSION 3.13)
project(air_quality C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(DRV       ${WHEELS}/drivers)

# ----- executable: air_quality -----
add_executable(air_quality
    air_quality.c

    # L1 - ccs811
    ${DRV}/ccs811/ccs811.c

    # L1 - sgp40
    ${DRV}/sgp40/sgp40.c

    # L1 - bme680
    ${DRV}/bme680/bme680.c

    # L1 - sht40
    ${DRV}/sht40/sht40.c

    # L1 - aht20
    ${DRV}/aht20/aht20.c

    # L1 - veml7700
    ${DRV}/veml7700/veml7700.c
)

target_include_directories(air_quality PRIVATE
    # wheel headers
    ${DRV}/ccs811
    ${DRV}/sgp40
    ${DRV}/bme680
    ${DRV}/sht40
    ${DRV}/aht20
    ${DRV}/veml7700

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(air_quality PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME air_quality_integration COMMAND air_quality)
