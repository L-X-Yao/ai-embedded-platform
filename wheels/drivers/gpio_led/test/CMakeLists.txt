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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)

target_compile_options(test_gpio_led PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_gpio_led
    test_null_gpio_led.c
    ../gpio_led.c
)
target_include_directories(test_null_gpio_led PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_gpio_led PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_gpio_led
    test_boundary_gpio_led.c
    ../gpio_led.c
)
target_include_directories(test_boundary_gpio_led PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_gpio_led PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME gpio_led_tests COMMAND test_gpio_led)

add_executable(prop_gpio_led
    prop_gpio_led.c
    ../gpio_led.c
)
target_include_directories(prop_gpio_led PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_gpio_led PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME gpio_led_prop_tests COMMAND prop_gpio_led)
add_test(NAME gpio_led_null_tests COMMAND test_null_gpio_led)
add_test(NAME gpio_led_boundary_tests COMMAND test_boundary_gpio_led)
