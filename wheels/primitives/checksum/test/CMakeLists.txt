cmake_minimum_required(VERSION 3.13)
project(test_checksum C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_checksum test_checksum.c ../checksum.c)
target_include_directories(test_checksum PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_checksum PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME checksum_tests COMMAND test_checksum)
