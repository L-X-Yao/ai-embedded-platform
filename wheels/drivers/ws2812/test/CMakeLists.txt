cmake_minimum_required(VERSION 3.13)
project(test_ws2812 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ws2812
    test_ws2812.c
    ../ws2812.c
)

target_include_directories(test_ws2812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

enable_testing()
add_test(NAME ws2812_tests COMMAND test_ws2812)
