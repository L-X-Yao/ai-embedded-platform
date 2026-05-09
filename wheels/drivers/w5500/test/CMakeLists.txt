cmake_minimum_required(VERSION 3.13)
project(test_w5500 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_w5500
    test_w5500.c
    ../w5500.c
)

target_include_directories(test_w5500 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_w5500 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME w5500_tests COMMAND test_w5500)

add_executable(test_null_w5500
    test_null_w5500.c
    ../w5500.c
)
target_include_directories(test_null_w5500 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_w5500 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME w5500_null_tests COMMAND test_null_w5500)
