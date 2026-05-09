cmake_minimum_required(VERSION 3.13)
project(test_lookup_table C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lookup_table
    test_lookup_table.c
    ../lookup_table.c
)

target_include_directories(test_lookup_table PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_lookup_table PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_lookup_table PRIVATE m)


add_executable(test_null_lookup_table
    test_null_lookup_table.c
    ../lookup_table.c
)
target_include_directories(test_null_lookup_table PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_lookup_table PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME lookup_table_tests COMMAND test_lookup_table)
add_test(NAME lookup_table_null_tests COMMAND test_null_lookup_table)
