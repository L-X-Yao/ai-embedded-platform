cmake_minimum_required(VERSION 3.13)
project(test_scheduler C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_scheduler
    test_scheduler.c
    ../scheduler.c
)
target_include_directories(test_scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_scheduler PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_scheduler
    test_null_scheduler.c
    ../scheduler.c
)
target_include_directories(test_null_scheduler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_scheduler PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME scheduler_tests COMMAND test_scheduler)
add_test(NAME scheduler_null_tests COMMAND test_null_scheduler)
