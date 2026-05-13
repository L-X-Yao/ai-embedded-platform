cmake_minimum_required(VERSION 3.13)
project(test_param_table C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_param_table
    test_param_table.c
    ../param_table.c
)

target_include_directories(test_param_table PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_param_table PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_param_table
    test_null_param_table.c
    ../param_table.c
)
target_include_directories(test_null_param_table PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_param_table PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_param_table
    test_boundary_param_table.c
    ../param_table.c
)
target_include_directories(test_boundary_param_table PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_param_table PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME param_table_tests COMMAND test_param_table)
add_test(NAME param_table_null_tests COMMAND test_null_param_table)
add_test(NAME param_table_boundary_tests COMMAND test_boundary_param_table)
