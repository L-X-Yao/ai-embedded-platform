cmake_minimum_required(VERSION 3.13)
project(test_pca9685 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_pca9685
    test_pca9685.c
    ../pca9685.c
)

target_include_directories(test_pca9685 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_pca9685 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME pca9685_tests COMMAND test_pca9685)

add_executable(prop_pca9685
    prop_pca9685.c
    ../pca9685.c
)
target_include_directories(prop_pca9685 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_pca9685 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME pca9685_prop_tests COMMAND prop_pca9685)
