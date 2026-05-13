cmake_minimum_required(VERSION 3.13)
project(test_mcp4728 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TOOLING_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness)

# --- main tests ---
add_executable(test_mcp4728
    test_mcp4728.c
    ../mcp4728.c
)
target_include_directories(test_mcp4728 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_mcp4728 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# --- hw_model datasheet test ---
add_executable(test_hw_model_mcp4728
    test_hw_model_mcp4728.c
    ../mcp4728.c
)
target_include_directories(test_hw_model_mcp4728 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_hw_model_mcp4728 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_mcp4728
    test_null_mcp4728.c
    ../mcp4728.c
)
target_include_directories(test_null_mcp4728 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_mcp4728 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_mcp4728
    test_boundary_mcp4728.c
    ../mcp4728.c
)
target_include_directories(test_boundary_mcp4728 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_mcp4728 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME mcp4728_tests COMMAND test_mcp4728)
add_test(NAME mcp4728_hw_model_tests COMMAND test_hw_model_mcp4728)
add_test(NAME mcp4728_null_tests COMMAND test_null_mcp4728)
add_test(NAME mcp4728_boundary_tests COMMAND test_boundary_mcp4728)
