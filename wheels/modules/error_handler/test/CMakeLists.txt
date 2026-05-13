cmake_minimum_required(VERSION 3.13)
project(test_error_handler C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_error_handler
    test_error_handler.c
    ../error_handler.c
)

target_include_directories(test_error_handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)

target_compile_options(test_error_handler PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_error_handler
    prop_error_handler.c
    ../error_handler.c
)
target_include_directories(prop_error_handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_error_handler PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)


add_executable(test_null_error_handler
    test_null_error_handler.c
    ../error_handler.c
)
target_include_directories(test_null_error_handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_error_handler PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_error_handler
    test_boundary_error_handler.c
    ../error_handler.c
)
target_include_directories(test_boundary_error_handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_error_handler PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME error_handler_tests COMMAND test_error_handler)
add_test(NAME error_handler_prop_tests COMMAND prop_error_handler)
add_test(NAME error_handler_null_tests COMMAND test_null_error_handler)
add_test(NAME error_handler_boundary_tests COMMAND test_boundary_error_handler)
