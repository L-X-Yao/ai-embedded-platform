cmake_minimum_required(VERSION 3.13)
project(test_protobuf_lite C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_protobuf_lite
    test_protobuf_lite.c
    ../protobuf_lite.c
)

target_include_directories(test_protobuf_lite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_protobuf_lite PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_protobuf_lite
    test_null_protobuf_lite.c
    ../protobuf_lite.c
)
target_include_directories(test_null_protobuf_lite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_protobuf_lite PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME protobuf_lite_tests COMMAND test_protobuf_lite)
add_test(NAME protobuf_lite_null_tests COMMAND test_null_protobuf_lite)
