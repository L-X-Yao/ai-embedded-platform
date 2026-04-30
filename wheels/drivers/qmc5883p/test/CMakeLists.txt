cmake_minimum_required(VERSION 3.13)
project(test_qmc5883p C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_qmc5883p
    test_qmc5883p.c
    ../qmc5883p.c
)

target_include_directories(test_qmc5883p PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_link_libraries(test_qmc5883p PRIVATE m)
target_compile_options(test_qmc5883p PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_test(NAME qmc5883p_tests COMMAND test_qmc5883p)
