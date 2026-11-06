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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_log PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_log
    test_null_log.c
    ../log.c
)
target_include_directories(test_null_log PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_log PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_log
    test_boundary_log.c
    ../log.c
)
target_include_directories(test_boundary_log PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_log PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME log_tests COMMAND test_log)
add_test(NAME log_null_tests COMMAND test_null_log)
add_test(NAME log_boundary_tests COMMAND test_boundary_log)

# log.c with printf_tiny as its formatter, which is optional.
add_executable(test_log_tiny
    test_log_tiny.c
    ../log.c
    ../../printf_tiny/printf_tiny.c
)
target_compile_definitions(test_log_tiny PRIVATE LOG_USE_PRINTF_TINY=1)
target_include_directories(test_log_tiny PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../printf_tiny
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_log_tiny PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME log_tiny_tests COMMAND test_log_tiny)

# One build per LOG_LEVEL_MAX, each told how many levels must survive.
function(add_log_max_test name max expected)
    add_executable(${name} test_log_max.c ../log.c)
    target_compile_definitions(${name} PRIVATE LOG_LEVEL_MAX=${max} EXPECTED_LEVELS=${expected})
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
    )
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_log_max_test(test_log_max_none   LOG_LEVEL_NONE  0)
add_log_max_test(test_log_max_error  LOG_LEVEL_ERROR 1)
add_log_max_test(test_log_max_warn   LOG_LEVEL_WARN  2)
add_log_max_test(test_log_max_info   LOG_LEVEL_INFO  3)
add_log_max_test(test_log_max_debug  LOG_LEVEL_DEBUG 4)
add_log_max_test(test_log_max_number 2               2)
