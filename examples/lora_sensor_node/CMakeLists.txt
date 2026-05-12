cmake_minimum_required(VERSION 3.13)
project(lora_sensor_node C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WHEELS ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(lora_sensor_node
    main.c
    ${WHEELS}/drivers/adxl345/adxl345.c
    ${WHEELS}/drivers/sx1262/sx1262.c
    ${WHEELS}/primitives/cobs/cobs.c
)

target_include_directories(lora_sensor_node PRIVATE
    ${WHEELS}/drivers/adxl345
    ${WHEELS}/drivers/sx1262
    ${WHEELS}/primitives/cobs
    ${TOOLING}/harness
)

target_compile_options(lora_sensor_node PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
