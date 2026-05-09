cmake_minimum_required(VERSION 3.13)
project(test_state_machine C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_state_machine
    test_state_machine.c
    ../state_machine.c
)

target_include_directories(test_state_machine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_state_machine PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_state_machine
    test_null_state_machine.c
    ../state_machine.c
)
target_include_directories(test_null_state_machine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_state_machine PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME state_machine_tests COMMAND test_state_machine)
add_test(NAME state_machine_null_tests COMMAND test_null_state_machine)
