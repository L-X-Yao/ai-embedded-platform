cmake_minimum_required(VERSION 3.13)
project(test_pcf8574 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_pcf8574
    test_pcf8574.c
    ../pcf8574.c
)

target_include_directories(test_pcf8574 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_pcf8574 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME pcf8574_tests COMMAND test_pcf8574)

add_executable(prop_pcf8574
    prop_pcf8574.c
    ../pcf8574.c
)
target_include_directories(prop_pcf8574 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_pcf8574 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME pcf8574_prop_tests COMMAND prop_pcf8574)
