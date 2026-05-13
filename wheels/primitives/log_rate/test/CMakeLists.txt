cmake_minimum_required(VERSION 3.13)
project(test_log_rate C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_log_rate
    test_log_rate.c
    ../log_rate.c
)

target_include_directories(test_log_rate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_log_rate PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_log_rate
    test_null_log_rate.c
    ../log_rate.c
)
target_include_directories(test_null_log_rate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_log_rate PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_log_rate
    test_boundary_log_rate.c
    ../log_rate.c
)
target_include_directories(test_boundary_log_rate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_log_rate PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME log_rate_tests COMMAND test_log_rate)
add_test(NAME log_rate_null_tests COMMAND test_null_log_rate)
add_test(NAME log_rate_boundary_tests COMMAND test_boundary_log_rate)
