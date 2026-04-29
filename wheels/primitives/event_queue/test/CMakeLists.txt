cmake_minimum_required(VERSION 3.13)
project(test_event_queue C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_event_queue
    test_event_queue.c
    ../event_queue.c
)

target_include_directories(test_event_queue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_options(test_event_queue PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME event_queue_tests COMMAND test_event_queue)
