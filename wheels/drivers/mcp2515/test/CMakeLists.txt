cmake_minimum_required(VERSION 3.13)
project(test_mcp2515 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mcp2515
    test_mcp2515.c
    ../mcp2515.c
)

target_include_directories(test_mcp2515 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mcp2515 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME mcp2515_tests COMMAND test_mcp2515)

add_executable(prop_mcp2515
    prop_mcp2515.c
    ../mcp2515.c
)
target_include_directories(prop_mcp2515 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_mcp2515 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME mcp2515_prop_tests COMMAND prop_mcp2515)
