cmake_minimum_required(VERSION 3.13)
project(test_sht30 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sht30
    test_sht30.c
    ../sht30.c
)

target_include_directories(test_sht30 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_sht30 PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(test_sht30 PRIVATE m)

enable_testing()
add_test(NAME sht30_tests COMMAND test_sht30)
