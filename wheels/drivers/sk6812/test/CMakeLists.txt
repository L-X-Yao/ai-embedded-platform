cmake_minimum_required(VERSION 3.13)
project(test_sk6812 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sk6812
    test_sk6812.c
    ../sk6812.c
)

target_include_directories(test_sk6812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_sk6812 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME sk6812_tests COMMAND test_sk6812)

add_executable(test_null_sk6812
    test_null_sk6812.c
    ../sk6812.c
)
target_include_directories(test_null_sk6812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_sk6812 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME sk6812_null_tests COMMAND test_null_sk6812)
