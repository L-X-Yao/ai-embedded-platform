cmake_minimum_required(VERSION 3.13)
project(test_ring_buffer C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ring_buffer
    test_ring_buffer.c
    ../ring_buffer.c
)

target_include_directories(test_ring_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_ring_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_ring_buffer
    test_null_ring_buffer.c
    ../ring_buffer.c
)
target_include_directories(test_null_ring_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_ring_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ring_buffer_tests COMMAND test_ring_buffer)
add_test(NAME ring_buffer_null_tests COMMAND test_null_ring_buffer)
