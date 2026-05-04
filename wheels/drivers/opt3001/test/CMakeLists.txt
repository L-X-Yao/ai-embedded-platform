cmake_minimum_required(VERSION 3.13)
project(test_opt3001 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_opt3001
    test_opt3001.c
    ../opt3001.c
)

target_include_directories(test_opt3001 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_opt3001 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME opt3001_tests COMMAND test_opt3001)

add_executable(prop_opt3001
    prop_opt3001.c
    ../opt3001.c
)
target_include_directories(prop_opt3001 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_opt3001 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME opt3001_prop_tests COMMAND prop_opt3001)
