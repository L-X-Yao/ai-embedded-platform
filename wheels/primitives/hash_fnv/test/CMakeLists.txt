cmake_minimum_required(VERSION 3.13)
project(test_hash_fnv C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hash_fnv
    test_hash_fnv.c
    ../hash_fnv.c
)

target_include_directories(test_hash_fnv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hash_fnv PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME hash_fnv_tests COMMAND test_hash_fnv)
