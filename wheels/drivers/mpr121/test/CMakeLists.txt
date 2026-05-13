cmake_minimum_required(VERSION 3.13)
project(test_mpr121 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TOOLING_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness)

# --- main tests ---
add_executable(test_mpr121
    test_mpr121.c
    ../mpr121.c
)
target_include_directories(test_mpr121 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_mpr121 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_mpr121 PRIVATE CONTRACT_ENABLED)

# --- null parameter tests ---
add_executable(test_null_mpr121
    test_null_mpr121.c
    ../mpr121.c
)
target_include_directories(test_null_mpr121 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_null_mpr121 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# --- hw_model datasheet test ---
add_executable(test_hw_model_mpr121
    test_hw_model_mpr121.c
    ../mpr121.c
)
target_include_directories(test_hw_model_mpr121 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_hw_model_mpr121 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_hw_model_mpr121 PRIVATE CONTRACT_ENABLED)


add_executable(test_boundary_mpr121
    test_boundary_mpr121.c
    ../mpr121.c
)
target_include_directories(test_boundary_mpr121 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_boundary_mpr121 PRIVATE CONTRACT_ENABLED)
target_compile_options(test_boundary_mpr121 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME mpr121_tests COMMAND test_mpr121)
add_test(NAME mpr121_null_tests COMMAND test_null_mpr121)
add_test(NAME mpr121_hw_model_tests COMMAND test_hw_model_mpr121)
add_test(NAME mpr121_boundary_tests COMMAND test_boundary_mpr121)
