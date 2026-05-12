cmake_minimum_required(VERSION 3.13)
project(display_hub C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(DRV       ${WHEELS}/drivers)

# ----- executable: display_hub -----
add_executable(display_hub
    display_hub.c

    # L1 - sh1106 (OLED, I2C)
    ${DRV}/sh1106/sh1106.c

    # L1 - hd44780 (character LCD, I2C via PCF8574)
    ${DRV}/hd44780/hd44780.c

    # L1 - tm1637 (7-segment, GPIO bit-bang)
    ${DRV}/tm1637/tm1637.c

    # L1 - max7219 (LED matrix, SPI)
    ${DRV}/max7219/max7219.c

    # L1 - lcd_st7789 (color TFT, SPI)
    ${DRV}/lcd_st7789/lcd_st7789.c

    # L1 - ili9341 (color TFT, SPI)
    ${DRV}/ili9341/ili9341.c
)

target_include_directories(display_hub PRIVATE
    # wheel headers
    ${DRV}/sh1106
    ${DRV}/hd44780
    ${DRV}/tm1637
    ${DRV}/max7219
    ${DRV}/lcd_st7789
    ${DRV}/ili9341

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(display_hub PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME display_hub_integration COMMAND display_hub)
