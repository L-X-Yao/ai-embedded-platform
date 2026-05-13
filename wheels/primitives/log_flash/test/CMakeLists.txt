cmake_minimum_required(VERSION 3.13)
project(test_log_flash C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_log_flash
    test_log_flash.c
    ../log_flash.c
)

target_include_directories(test_log_flash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_log_flash PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_log_flash
    test_null_log_flash.c
    ../log_flash.c
)
target_include_directories(test_null_log_flash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_log_flash PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_log_flash
    test_boundary_log_flash.c
    ../log_flash.c
)
target_include_directories(test_boundary_log_flash PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_log_flash PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME log_flash_tests COMMAND test_log_flash)
add_test(NAME log_flash_null_tests COMMAND test_null_log_flash)
add_test(NAME log_flash_boundary_tests COMMAND test_boundary_log_flash)
