cmake_minimum_required(VERSION 3.13)
project(test_hmc5883l C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hmc5883l
    test_hmc5883l.c
    ../hmc5883l.c
)
target_include_directories(test_hmc5883l PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_options(test_hmc5883l PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(test_hmc5883l PRIVATE m)

enable_testing()
add_test(NAME hmc5883l_tests COMMAND test_hmc5883l)
