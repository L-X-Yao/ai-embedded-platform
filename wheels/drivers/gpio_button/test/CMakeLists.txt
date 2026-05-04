cmake_minimum_required(VERSION 3.13)
project(test_gpio_button C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_gpio_button
    test_gpio_button.c
    ../gpio_button.c
)

target_include_directories(test_gpio_button PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_compile_options(test_gpio_button PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_gpio_button
    prop_gpio_button.c
    ../gpio_button.c
)
target_include_directories(prop_gpio_button PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_gpio_button PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)

enable_testing()
add_test(NAME gpio_button_tests COMMAND test_gpio_button)
add_test(NAME gpio_button_prop_tests COMMAND prop_gpio_button)
