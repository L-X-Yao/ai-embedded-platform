cmake_minimum_required(VERSION 3.13)
project(test_lis3dh C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lis3dh
    test_lis3dh.c
    ../lis3dh.c
)

target_include_directories(test_lis3dh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_lis3dh PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME lis3dh_tests COMMAND test_lis3dh)
