cmake_minimum_required(VERSION 3.13)
project(test_ina219 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ina219
    test_ina219.c
    ../ina219.c
)

target_include_directories(test_ina219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_ina219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ina219 PRIVATE m)

enable_testing()
add_test(NAME ina219_tests COMMAND test_ina219)
