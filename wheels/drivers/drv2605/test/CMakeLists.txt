cmake_minimum_required(VERSION 3.13)
project(test_drv2605 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_drv2605
    test_drv2605.c
    ../drv2605.c
)

target_include_directories(test_drv2605 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_drv2605 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_drv2605
    test_hw_model_drv2605.c
    ../drv2605.c
)
target_include_directories(test_hw_model_drv2605 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_drv2605 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-conversion)

enable_testing()
add_test(NAME drv2605_tests COMMAND test_drv2605)
add_test(NAME drv2605_hw_model_tests COMMAND test_hw_model_drv2605)

add_executable(prop_drv2605
    prop_drv2605.c
    ../drv2605.c
)
target_include_directories(prop_drv2605 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_drv2605 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME drv2605_prop_tests COMMAND prop_drv2605)
