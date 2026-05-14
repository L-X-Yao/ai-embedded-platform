cmake_minimum_required(VERSION 3.13)
project(test_retry C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_retry
    test_retry.c
    ../retry.c
)
target_include_directories(test_retry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_retry PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_retry
    test_null_retry.c
    ../retry.c
)
target_include_directories(test_null_retry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_retry PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_retry
    test_boundary_retry.c
    ../retry.c
)
target_include_directories(test_boundary_retry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_retry PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME retry_tests COMMAND test_retry)
add_test(NAME retry_null_tests COMMAND test_null_retry)
add_test(NAME retry_boundary_tests COMMAND test_boundary_retry)
