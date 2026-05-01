cmake_minimum_required(VERSION 3.13)
project(test_stepper C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_stepper
    test_stepper.c
    ../stepper.c
)

target_include_directories(test_stepper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_stepper PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME stepper_tests COMMAND test_stepper)
