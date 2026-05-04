cmake_minimum_required(VERSION 3.13)
project(test_drv8833 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_drv8833
    test_drv8833.c
    ../drv8833.c
)

target_include_directories(test_drv8833 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_drv8833 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME drv8833_tests COMMAND test_drv8833)

add_executable(prop_drv8833
    prop_drv8833.c
    ../drv8833.c
)
target_include_directories(prop_drv8833 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_drv8833 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME drv8833_prop_tests COMMAND prop_drv8833)
