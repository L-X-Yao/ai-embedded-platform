cmake_minimum_required(VERSION 3.13)
project(test_rle_codec C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_rle_codec
    test_rle_codec.c
    ../rle_codec.c
)

target_include_directories(test_rle_codec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_rle_codec PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_rle_codec
    test_null_rle_codec.c
    ../rle_codec.c
)
target_include_directories(test_null_rle_codec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_rle_codec PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME rle_codec_tests COMMAND test_rle_codec)
add_test(NAME rle_codec_null_tests COMMAND test_null_rle_codec)
