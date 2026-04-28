cmake_minimum_required(VERSION 3.13)
project(test_gpio_led C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_gpio_led
    test_gpio_led.c
    ../gpio_led.c
)

target_include_directories(test_gpio_led PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

enable_testing()
add_test(NAME gpio_led_tests COMMAND test_gpio_led)
