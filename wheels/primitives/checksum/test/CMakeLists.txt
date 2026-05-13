cmake_minimum_required(VERSION 3.13)
project(test_checksum C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_checksum test_checksum.c ../checksum.c)
target_include_directories(test_checksum PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_checksum PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_checksum
    test_null_checksum.c
    ../checksum.c
)
target_include_directories(test_null_checksum PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_checksum PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_checksum
    test_boundary_checksum.c
    ../checksum.c
)
target_include_directories(test_boundary_checksum PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_checksum PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME checksum_tests COMMAND test_checksum)
add_test(NAME checksum_null_tests COMMAND test_null_checksum)
add_test(NAME checksum_boundary_tests COMMAND test_boundary_checksum)
