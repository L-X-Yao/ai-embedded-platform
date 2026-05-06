cmake_minimum_required(VERSION 3.13)
project(test_ft6x36 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TOOLING_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test)

# --- main tests ---
add_executable(test_ft6x36
    test_ft6x36.c
    ../ft6x36.c
)
target_include_directories(test_ft6x36 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_ft6x36 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# --- null parameter tests ---
add_executable(test_null_ft6x36
    test_null_ft6x36.c
    ../ft6x36.c
)
target_include_directories(test_null_ft6x36 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_null_ft6x36 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# --- hardware model tests ---
add_executable(test_hw_model_ft6x36
    test_hw_model_ft6x36.c
    ../ft6x36.c
)
target_include_directories(test_hw_model_ft6x36 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_hw_model_ft6x36 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ft6x36_tests COMMAND test_ft6x36)
add_test(NAME ft6x36_null_tests COMMAND test_null_ft6x36)
add_test(NAME ft6x36_hw_model_tests COMMAND test_hw_model_ft6x36)
