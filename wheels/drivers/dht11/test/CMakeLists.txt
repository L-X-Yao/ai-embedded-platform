cmake_minimum_required(VERSION 3.13)
project(test_dht11 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_dht11
    test_dht11.c
    ../dht11.c
)
target_include_directories(test_dht11 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_dht11 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME dht11_tests COMMAND test_dht11)
