cmake_minimum_required(VERSION 3.13)
project(test_hx711 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hx711
    test_hx711.c
    ../hx711.c
)

target_include_directories(test_hx711 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hx711 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME hx711_tests COMMAND test_hx711)
