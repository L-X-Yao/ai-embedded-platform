cmake_minimum_required(VERSION 3.13)
project(test_version_info C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_version_info
    test_version_info.c
    ../version_info.c
)

target_include_directories(test_version_info PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_version_info PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME version_info_tests COMMAND test_version_info)
