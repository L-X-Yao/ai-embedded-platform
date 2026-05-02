cmake_minimum_required(VERSION 3.13)
project(test_string_utils C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_string_utils
    test_string_utils.c
    ../string_utils.c
)

target_include_directories(test_string_utils PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_string_utils PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME string_utils_tests COMMAND test_string_utils)
