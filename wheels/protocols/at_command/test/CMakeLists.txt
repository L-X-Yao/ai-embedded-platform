cmake_minimum_required(VERSION 3.13)
project(test_at_command C)

add_executable(test_at_command
    test_at_command.c
    ../at_command.c
)

target_include_directories(test_at_command PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

enable_testing()
add_test(NAME at_command_tests COMMAND test_at_command)
