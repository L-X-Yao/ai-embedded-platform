cmake_minimum_required(VERSION 3.13)
project(test_bencode C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bencode
    test_bencode.c
    ../bencode.c
)

target_include_directories(test_bencode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bencode PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_bencode
    test_null_bencode.c
    ../bencode.c
)
target_include_directories(test_null_bencode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bencode PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_bencode
    test_boundary_bencode.c
    ../bencode.c
)
target_include_directories(test_boundary_bencode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_bencode PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bencode_tests COMMAND test_bencode)
add_test(NAME bencode_null_tests COMMAND test_null_bencode)
add_test(NAME bencode_boundary_tests COMMAND test_boundary_bencode)
