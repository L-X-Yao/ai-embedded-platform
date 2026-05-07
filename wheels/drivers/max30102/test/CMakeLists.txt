cmake_minimum_required(VERSION 3.13)
project(test_max30102 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_max30102
    test_max30102.c
    ../max30102.c
)

target_include_directories(test_max30102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_max30102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_max30102 PRIVATE m)

enable_testing()
add_test(NAME max30102_tests COMMAND test_max30102)
