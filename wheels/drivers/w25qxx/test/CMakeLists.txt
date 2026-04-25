cmake_minimum_required(VERSION 3.13)
project(test_w25qxx C)

add_executable(test_w25qxx
    test_w25qxx.c
    ../w25qxx.c
)

target_include_directories(test_w25qxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

enable_testing()
add_test(NAME w25qxx_tests COMMAND test_w25qxx)
