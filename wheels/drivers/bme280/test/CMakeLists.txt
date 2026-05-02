cmake_minimum_required(VERSION 3.13)
project(test_bme280 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bme280
    test_bme280.c
    ../bme280.c
)

target_include_directories(test_bme280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_bme280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_bme280 PRIVATE m)

enable_testing()
add_test(NAME bme280_tests COMMAND test_bme280)
