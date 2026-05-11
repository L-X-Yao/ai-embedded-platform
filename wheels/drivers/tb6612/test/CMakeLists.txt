cmake_minimum_required(VERSION 3.13)
project(test_tb6612 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_tb6612
    test_tb6612.c
    ../tb6612.c
)

target_include_directories(test_tb6612 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_tb6612 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME tb6612_tests COMMAND test_tb6612)

add_executable(test_null_tb6612
    test_null_tb6612.c
    ../tb6612.c
)
target_include_directories(test_null_tb6612 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_tb6612 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME tb6612_null_tests COMMAND test_null_tb6612)