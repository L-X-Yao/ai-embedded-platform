cmake_minimum_required(VERSION 3.13)
project(test_hd44780 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hd44780
    test_hd44780.c
    ../hd44780.c
)

target_include_directories(test_hd44780 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hd44780 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_hd44780
    test_null_hd44780.c
    ../hd44780.c
)
target_include_directories(test_null_hd44780 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_hd44780 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_hd44780
    test_boundary_hd44780.c
    ../hd44780.c
)
target_include_directories(test_boundary_hd44780 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_hd44780 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME hd44780_tests COMMAND test_hd44780)

# hw_model datasheet test
add_executable(test_hw_model_hd44780
    test_hw_model_hd44780.c
    ../hd44780.c
)
target_include_directories(test_hw_model_hd44780 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_hd44780 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
add_test(NAME hd44780_hw_model_tests COMMAND test_hw_model_hd44780)
add_test(NAME hd44780_null_tests COMMAND test_null_hd44780)
add_test(NAME hd44780_boundary_tests COMMAND test_boundary_hd44780)
