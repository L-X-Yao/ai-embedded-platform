cmake_minimum_required(VERSION 3.13)
project(test_bmp280 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bmp280
    test_bmp280.c
    ../bmp280.c
)

target_include_directories(test_bmp280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_bmp280 PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(test_bmp280 PRIVATE m)

enable_testing()
add_test(NAME bmp280_tests COMMAND test_bmp280)
