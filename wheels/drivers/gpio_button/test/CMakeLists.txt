cmake_minimum_required(VERSION 3.13)
project(test_gpio_button C)

add_executable(test_gpio_button
    test_gpio_button.c
    ../gpio_button.c
)

target_include_directories(test_gpio_button PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

enable_testing()
add_test(NAME gpio_button_tests COMMAND test_gpio_button)
