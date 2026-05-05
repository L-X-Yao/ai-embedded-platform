cmake_minimum_required(VERSION 3.13)
project(test_mcp4725 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TOOLING_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test)

# --- main tests ---
add_executable(test_mcp4725
    test_mcp4725.c
    ../mcp4725.c
)
target_include_directories(test_mcp4725 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_mcp4725 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# --- null parameter tests ---
add_executable(test_null_mcp4725
    test_null_mcp4725.c
    ../mcp4725.c
)
target_include_directories(test_null_mcp4725 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_null_mcp4725 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# --- property-based tests ---
add_executable(prop_mcp4725
    prop_mcp4725.c
    ../mcp4725.c
)
target_include_directories(prop_mcp4725 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(prop_mcp4725 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# --- hardware model tests ---
add_executable(test_hw_model_mcp4725
    test_hw_model_mcp4725.c
    ../mcp4725.c
)
target_include_directories(test_hw_model_mcp4725 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_hw_model_mcp4725 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME mcp4725_tests COMMAND test_mcp4725)
add_test(NAME mcp4725_null_tests COMMAND test_null_mcp4725)
add_test(NAME mcp4725_prop_tests COMMAND prop_mcp4725)
add_test(NAME mcp4725_hw_model_tests COMMAND test_hw_model_mcp4725)
