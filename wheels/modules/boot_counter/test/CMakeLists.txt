cmake_minimum_required(VERSION 3.13)
project(test_boot_counter C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_boot_counter
    test_boot_counter.c
    ../boot_counter.c
)

target_include_directories(test_boot_counter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_boot_counter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME boot_counter_tests COMMAND test_boot_counter)
