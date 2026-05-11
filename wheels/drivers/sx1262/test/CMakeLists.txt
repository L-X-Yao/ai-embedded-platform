cmake_minimum_required(VERSION 3.13)
project(test_sx1262 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sx1262
    test_sx1262.c
    ../sx1262.c
)

target_include_directories(test_sx1262 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_sx1262 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME sx1262_tests COMMAND test_sx1262)

add_executable(test_null_sx1262
    test_null_sx1262.c
    ../sx1262.c
)
target_include_directories(test_null_sx1262 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_sx1262 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME sx1262_null_tests COMMAND test_null_sx1262)
