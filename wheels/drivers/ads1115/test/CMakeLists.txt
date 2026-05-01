cmake_minimum_required(VERSION 3.13)
project(test_ads1115 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ads1115
    test_ads1115.c
    ../ads1115.c
)
target_include_directories(test_ads1115 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_ads1115 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ads1115 PRIVATE m)

enable_testing()
add_test(NAME ads1115_tests COMMAND test_ads1115)
