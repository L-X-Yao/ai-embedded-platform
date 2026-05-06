cmake_minimum_required(VERSION 3.13)
project(test_sgp30 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TOOLING_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test)

# --- main tests ---
add_executable(test_sgp30
    test_sgp30.c
    ../sgp30.c
)
target_include_directories(test_sgp30 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_sgp30 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# --- null parameter tests ---
add_executable(test_null_sgp30
    test_null_sgp30.c
    ../sgp30.c
)
target_include_directories(test_null_sgp30 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_null_sgp30 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME sgp30_tests COMMAND test_sgp30)
add_test(NAME sgp30_null_tests COMMAND test_null_sgp30)
