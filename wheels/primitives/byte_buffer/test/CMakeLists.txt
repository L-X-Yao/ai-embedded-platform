cmake_minimum_required(VERSION 3.13)
project(test_byte_buffer C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_byte_buffer
    test_byte_buffer.c
    ../byte_buffer.c
)

target_include_directories(test_byte_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_options(test_byte_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME byte_buffer_tests COMMAND test_byte_buffer)
