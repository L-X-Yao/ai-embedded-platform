cmake_minimum_required(VERSION 3.13)
project(test_sh1106 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sh1106
    test_sh1106.c
    ../sh1106.c
)

target_include_directories(test_sh1106 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_sh1106 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_sh1106
    test_null_sh1106.c
    ../sh1106.c
)
target_include_directories(test_null_sh1106 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_sh1106 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME sh1106_tests COMMAND test_sh1106)

# hw_model datasheet test
add_executable(test_hw_model_sh1106
    test_hw_model_sh1106.c
    ../sh1106.c
)
target_include_directories(test_hw_model_sh1106 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_sh1106 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
add_test(NAME sh1106_hw_model_tests COMMAND test_hw_model_sh1106)
add_test(NAME sh1106_null_tests COMMAND test_null_sh1106)
