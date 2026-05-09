cmake_minimum_required(VERSION 3.13)
project(test_button_handler C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_button_handler test_button_handler.c ../button_handler.c)
target_include_directories(test_button_handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_button_handler PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_button_handler
    test_null_button_handler.c
    ../button_handler.c
)
target_include_directories(test_null_button_handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_button_handler PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME button_handler_tests COMMAND test_button_handler)
add_test(NAME button_handler_null_tests COMMAND test_null_button_handler)
