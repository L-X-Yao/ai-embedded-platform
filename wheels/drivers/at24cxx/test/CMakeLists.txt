cmake_minimum_required(VERSION 3.13)
project(test_at24cxx C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_at24cxx
    test_at24cxx.c
    ../at24cxx.c
)

target_include_directories(test_at24cxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_at24cxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_at24cxx
    prop_at24cxx.c
    ../at24cxx.c
)
target_include_directories(prop_at24cxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_at24cxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)

enable_testing()
add_test(NAME at24cxx_tests COMMAND test_at24cxx)
add_test(NAME at24cxx_prop_tests COMMAND prop_at24cxx)
