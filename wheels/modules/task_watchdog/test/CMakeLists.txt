cmake_minimum_required(VERSION 3.13)
project(test_task_watchdog C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_task_watchdog
    test_task_watchdog.c
    ../task_watchdog.c
)

target_include_directories(test_task_watchdog PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_task_watchdog PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME task_watchdog_tests COMMAND test_task_watchdog)
