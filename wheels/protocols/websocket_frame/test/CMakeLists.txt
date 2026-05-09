cmake_minimum_required(VERSION 3.13)
project(test_websocket_frame C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_websocket_frame
    test_websocket_frame.c
    ../websocket_frame.c
)
target_include_directories(test_websocket_frame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_websocket_frame PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_websocket_frame
    test_null_websocket_frame.c
    ../websocket_frame.c
)
target_include_directories(test_null_websocket_frame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_websocket_frame PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME websocket_frame_tests COMMAND test_websocket_frame)
add_test(NAME websocket_frame_null_tests COMMAND test_null_websocket_frame)
