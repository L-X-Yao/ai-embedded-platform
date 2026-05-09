cmake_minimum_required(VERSION 3.13)
project(test_slip C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_slip
    test_slip.c
    ../slip.c
)

target_include_directories(test_slip PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_slip PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_slip
    test_null_slip.c
    ../slip.c
)
target_include_directories(test_null_slip PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_slip PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME slip_tests COMMAND test_slip)
add_test(NAME slip_null_tests COMMAND test_null_slip)
