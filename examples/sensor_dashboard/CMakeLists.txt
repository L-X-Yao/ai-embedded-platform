cmake_minimum_required(VERSION 3.13)
project(sensor_dashboard C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WHEELS ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(sensor_dashboard
    main.c
    ${WHEELS}/primitives/log/log.c
    ${WHEELS}/drivers/bmp280/bmp280.c
    ${WHEELS}/drivers/sht30/sht30.c
)

target_include_directories(sensor_dashboard PRIVATE
    ${WHEELS}/primitives/log
    ${WHEELS}/drivers/bmp280
    ${WHEELS}/drivers/sht30
    ${TOOLING}/harness
)

target_compile_options(sensor_dashboard PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
