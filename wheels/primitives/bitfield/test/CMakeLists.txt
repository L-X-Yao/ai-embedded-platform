cmake_minimum_required(VERSION 3.13)
project(test_bitfield C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bitfield
    test_bitfield.c
    ../bitfield.c
)
target_include_directories(test_bitfield PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bitfield PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_bitfield
    test_null_bitfield.c
    ../bitfield.c
)
target_include_directories(test_null_bitfield PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bitfield PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bitfield_tests COMMAND test_bitfield)
add_test(NAME bitfield_null_tests COMMAND test_null_bitfield)
