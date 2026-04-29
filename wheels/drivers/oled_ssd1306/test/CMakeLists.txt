cmake_minimum_required(VERSION 3.13)
project(test_oled_ssd1306 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_oled_ssd1306
    test_oled_ssd1306.c
    ../oled_ssd1306.c
)

target_include_directories(test_oled_ssd1306 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_options(test_oled_ssd1306 PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME oled_ssd1306_tests COMMAND test_oled_ssd1306)
