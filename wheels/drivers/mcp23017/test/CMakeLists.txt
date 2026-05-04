cmake_minimum_required(VERSION 3.13)
project(test_mcp23017 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mcp23017
    test_mcp23017.c
    ../mcp23017.c
)

target_include_directories(test_mcp23017 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mcp23017 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME mcp23017_tests COMMAND test_mcp23017)

add_executable(prop_mcp23017
    prop_mcp23017.c
    ../mcp23017.c
)
target_include_directories(prop_mcp23017 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_mcp23017 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME mcp23017_prop_tests COMMAND prop_mcp23017)
