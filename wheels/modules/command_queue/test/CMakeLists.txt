cmake_minimum_required(VERSION 3.13)
project(test_command_queue C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_command_queue
    test_command_queue.c
    ../command_queue.c
)

target_include_directories(test_command_queue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_command_queue PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_command_queue
    test_null_command_queue.c
    ../command_queue.c
)
target_include_directories(test_null_command_queue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_command_queue PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME command_queue_tests COMMAND test_command_queue)
add_test(NAME command_queue_null_tests COMMAND test_null_command_queue)
