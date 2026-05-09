cmake_minimum_required(VERSION 3.13)
project(test_crc32 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_crc32
    test_crc32.c
    ../crc32.c
)
target_include_directories(test_crc32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_crc32 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_crc32
    test_null_crc32.c
    ../crc32.c
)
target_include_directories(test_null_crc32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_crc32 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME crc32_tests COMMAND test_crc32)
add_test(NAME crc32_null_tests COMMAND test_null_crc32)
