cmake_minimum_required(VERSION 3.13)
project(test_keypad_matrix C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_keypad_matrix
    test_keypad_matrix.c
    ../keypad_matrix.c
)

target_include_directories(test_keypad_matrix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_keypad_matrix PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_keypad_matrix
    test_null_keypad_matrix.c
    ../keypad_matrix.c
)
target_include_directories(test_null_keypad_matrix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_keypad_matrix PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME keypad_matrix_tests COMMAND test_keypad_matrix)

add_executable(prop_keypad_matrix
    prop_keypad_matrix.c
    ../keypad_matrix.c
)
target_include_directories(prop_keypad_matrix PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_keypad_matrix PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME keypad_matrix_prop_tests COMMAND prop_keypad_matrix)
add_test(NAME keypad_matrix_null_tests COMMAND test_null_keypad_matrix)
