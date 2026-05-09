cmake_minimum_required(VERSION 3.13)
project(test_log C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_log
    test_log.c
    ../log.c
)
target_include_directories(test_log PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_log PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_log
    test_null_log.c
    ../log.c
)
target_include_directories(test_null_log PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_log PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME log_tests COMMAND test_log)
add_test(NAME log_null_tests COMMAND test_null_log)
