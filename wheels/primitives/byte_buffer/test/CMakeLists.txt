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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_byte_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_byte_buffer
    test_null_byte_buffer.c
    ../byte_buffer.c
)
target_include_directories(test_null_byte_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_byte_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME byte_buffer_tests COMMAND test_byte_buffer)
add_test(NAME byte_buffer_null_tests COMMAND test_null_byte_buffer)
