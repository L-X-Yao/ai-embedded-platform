cmake_minimum_required(VERSION 3.13)
project(test_kalman_1d C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TOOLING_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness)

# --- main tests ---
add_executable(test_kalman_1d
    test_kalman_1d.c
    ../kalman_1d.c
)
target_include_directories(test_kalman_1d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_kalman_1d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_kalman_1d PRIVATE m)

# --- null parameter tests ---
add_executable(test_null_kalman_1d
    test_null_kalman_1d.c
    ../kalman_1d.c
)
target_include_directories(test_null_kalman_1d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_null_kalman_1d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME kalman_1d_tests      COMMAND test_kalman_1d)
add_test(NAME kalman_1d_null_tests COMMAND test_null_kalman_1d)
