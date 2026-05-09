cmake_minimum_required(VERSION 3.13)
project(test_rate_limiter C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_rate_limiter
    test_rate_limiter.c
    ../rate_limiter.c
)

target_include_directories(test_rate_limiter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_rate_limiter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_rate_limiter
    test_null_rate_limiter.c
    ../rate_limiter.c
)
target_include_directories(test_null_rate_limiter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_rate_limiter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME rate_limiter_tests COMMAND test_rate_limiter)
add_test(NAME rate_limiter_null_tests COMMAND test_null_rate_limiter)
