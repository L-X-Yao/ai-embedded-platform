cmake_minimum_required(VERSION 3.13)
project(test_bitmap C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bitmap
    test_bitmap.c
    ../bitmap.c
)

target_include_directories(test_bitmap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_bitmap PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_bitmap
    test_null_bitmap.c
    ../bitmap.c
)
target_include_directories(test_null_bitmap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_bitmap PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bitmap_tests COMMAND test_bitmap)
add_test(NAME bitmap_null_tests COMMAND test_null_bitmap)
