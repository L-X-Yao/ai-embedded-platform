cmake_minimum_required(VERSION 3.13)
project(test_led_pattern C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_led_pattern test_led_pattern.c ../led_pattern.c)
target_include_directories(test_led_pattern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_led_pattern PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_led_pattern
    test_null_led_pattern.c
    ../led_pattern.c
)
target_include_directories(test_null_led_pattern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_led_pattern PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_led_pattern
    test_boundary_led_pattern.c
    ../led_pattern.c
)
target_include_directories(test_boundary_led_pattern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_led_pattern PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME led_pattern_tests COMMAND test_led_pattern)
add_test(NAME led_pattern_null_tests COMMAND test_null_led_pattern)
add_test(NAME led_pattern_boundary_tests COMMAND test_boundary_led_pattern)
