cmake_minimum_required(VERSION 3.13)
project(test_ota_ab C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ota_ab
    test_ota_ab.c
    ../ota_ab.c
)

target_include_directories(test_ota_ab PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_ota_ab PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ota_ab_tests COMMAND test_ota_ab)
