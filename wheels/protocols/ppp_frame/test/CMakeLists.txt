cmake_minimum_required(VERSION 3.13)
project(test_ppp_frame C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ppp_frame
    test_ppp_frame.c
    ../ppp_frame.c
)

target_include_directories(test_ppp_frame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ppp_frame PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_ppp_frame
    test_null_ppp_frame.c
    ../ppp_frame.c
)
target_include_directories(test_null_ppp_frame PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ppp_frame PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ppp_frame_tests COMMAND test_ppp_frame)
add_test(NAME ppp_frame_null_tests COMMAND test_null_ppp_frame)
