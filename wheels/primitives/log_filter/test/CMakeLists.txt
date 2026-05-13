cmake_minimum_required(VERSION 3.13)
project(test_log_filter C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_log_filter
    test_log_filter.c
    ../log_filter.c
)

target_include_directories(test_log_filter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_log_filter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_log_filter
    test_null_log_filter.c
    ../log_filter.c
)
target_include_directories(test_null_log_filter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_log_filter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_log_filter
    test_boundary_log_filter.c
    ../log_filter.c
)
target_include_directories(test_boundary_log_filter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_log_filter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME log_filter_tests COMMAND test_log_filter)
add_test(NAME log_filter_null_tests COMMAND test_null_log_filter)
add_test(NAME log_filter_boundary_tests COMMAND test_boundary_log_filter)
