cmake_minimum_required(VERSION 3.13)
project(test_hex_codec C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hex_codec
    test_hex_codec.c
    ../hex_codec.c
)

target_include_directories(test_hex_codec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hex_codec PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_hex_codec
    test_null_hex_codec.c
    ../hex_codec.c
)
target_include_directories(test_null_hex_codec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_hex_codec PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_hex_codec
    test_boundary_hex_codec.c
    ../hex_codec.c
)
target_include_directories(test_boundary_hex_codec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_hex_codec PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME hex_codec_tests COMMAND test_hex_codec)
add_test(NAME hex_codec_null_tests COMMAND test_null_hex_codec)
add_test(NAME hex_codec_boundary_tests COMMAND test_boundary_hex_codec)
