cmake_minimum_required(VERSION 3.13)
project(test_soft_timer C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_soft_timer
    test_soft_timer.c
    ../soft_timer.c
)

target_include_directories(test_soft_timer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_options(test_soft_timer PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME soft_timer_tests COMMAND test_soft_timer)
