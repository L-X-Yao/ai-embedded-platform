cmake_minimum_required(VERSION 3.13)
project(test_fixed_point C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_fixed_point
    test_fixed_point.c
    ../fixed_point.c
)

target_include_directories(test_fixed_point PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_fixed_point PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_fixed_point
    test_null_fixed_point.c
    ../fixed_point.c
)
target_include_directories(test_null_fixed_point PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_fixed_point PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME fixed_point_tests COMMAND test_fixed_point)
add_test(NAME fixed_point_null_tests COMMAND test_null_fixed_point)
