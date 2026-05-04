cmake_minimum_required(VERSION 3.13)
project(test_w25qxx C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_w25qxx
    test_w25qxx.c
    ../w25qxx.c
)

target_include_directories(test_w25qxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_compile_options(test_w25qxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME w25qxx_tests COMMAND test_w25qxx)

add_executable(prop_w25qxx
    prop_w25qxx.c
    ../w25qxx.c
)
target_include_directories(prop_w25qxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_w25qxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME w25qxx_prop_tests COMMAND prop_w25qxx)
