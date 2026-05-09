cmake_minimum_required(VERSION 3.13)
project(test_timer_wheel C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_timer_wheel
    test_timer_wheel.c
    ../timer_wheel.c
)

target_include_directories(test_timer_wheel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_timer_wheel PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_timer_wheel
    test_null_timer_wheel.c
    ../timer_wheel.c
)
target_include_directories(test_null_timer_wheel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_timer_wheel PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME timer_wheel_tests COMMAND test_timer_wheel)
add_test(NAME timer_wheel_null_tests COMMAND test_null_timer_wheel)
