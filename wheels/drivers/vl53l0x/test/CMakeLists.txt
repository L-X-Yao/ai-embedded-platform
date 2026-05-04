cmake_minimum_required(VERSION 3.13)
project(test_vl53l0x C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_vl53l0x
    test_vl53l0x.c
    ../vl53l0x.c
)

target_include_directories(test_vl53l0x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_vl53l0x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME vl53l0x_tests COMMAND test_vl53l0x)

add_executable(prop_vl53l0x
    prop_vl53l0x.c
    ../vl53l0x.c
)
target_include_directories(prop_vl53l0x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_vl53l0x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME vl53l0x_prop_tests COMMAND prop_vl53l0x)
