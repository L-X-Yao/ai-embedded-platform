cmake_minimum_required(VERSION 3.13)
project(test_veml7700 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_veml7700
    test_veml7700.c
    ../veml7700.c
)

target_include_directories(test_veml7700 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_veml7700 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME veml7700_tests COMMAND test_veml7700)
