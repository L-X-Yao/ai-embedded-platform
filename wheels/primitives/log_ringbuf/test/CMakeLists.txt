cmake_minimum_required(VERSION 3.13)
project(test_log_ringbuf C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_log_ringbuf
    test_log_ringbuf.c
    ../log_ringbuf.c
)

target_include_directories(test_log_ringbuf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_log_ringbuf PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_log_ringbuf
    test_null_log_ringbuf.c
    ../log_ringbuf.c
)
target_include_directories(test_null_log_ringbuf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_log_ringbuf PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME log_ringbuf_tests COMMAND test_log_ringbuf)
add_test(NAME log_ringbuf_null_tests COMMAND test_null_log_ringbuf)
