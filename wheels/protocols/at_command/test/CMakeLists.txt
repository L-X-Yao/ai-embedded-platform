cmake_minimum_required(VERSION 3.13)
project(test_at_command C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_at_command
    test_at_command.c
    ../at_command.c
)

target_include_directories(test_at_command PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)

target_compile_options(test_at_command PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_at_command
    test_null_at_command.c
    ../at_command.c
)
target_include_directories(test_null_at_command PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_at_command PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_at_command
    test_boundary_at_command.c
    ../at_command.c
)
target_include_directories(test_boundary_at_command PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_at_command PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME at_command_tests COMMAND test_at_command)
add_test(NAME at_command_null_tests COMMAND test_null_at_command)
add_test(NAME at_command_boundary_tests COMMAND test_boundary_at_command)
