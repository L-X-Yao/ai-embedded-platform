cmake_minimum_required(VERSION 3.13)
project(oled_display C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(WHEELS ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

add_executable(oled_display
    main.c
    ${WHEELS}/primitives/printf_tiny/printf_tiny.c
    ${WHEELS}/primitives/font_5x7/font_5x7.c
    ${WHEELS}/drivers/oled_ssd1306/oled_ssd1306.c
    ${WHEELS}/drivers/bmp280/bmp280.c
)

target_include_directories(oled_display PRIVATE
    ${WHEELS}/primitives/printf_tiny
    ${WHEELS}/primitives/font_5x7
    ${WHEELS}/drivers/oled_ssd1306
    ${WHEELS}/drivers/bmp280
    ${TOOLING}/harness
)

target_compile_options(oled_display PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)
