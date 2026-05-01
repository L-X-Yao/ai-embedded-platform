cmake_minimum_required(VERSION 3.13)
project(test_cli_shell C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_cli_shell
    test_cli_shell.c
    ../cli_shell.c
)

target_include_directories(test_cli_shell PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_compile_options(test_cli_shell PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME cli_shell_tests COMMAND test_cli_shell)
