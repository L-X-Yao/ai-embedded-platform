cmake_minimum_required(VERSION 3.13)
project(test_tcs34725 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_tcs34725
    test_tcs34725.c
    ../tcs34725.c
)

target_include_directories(test_tcs34725 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_tcs34725 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_tcs34725 PRIVATE m)

enable_testing()
add_test(NAME tcs34725_tests COMMAND test_tcs34725)

add_executable(prop_tcs34725
    prop_tcs34725.c
    ../tcs34725.c
)
target_include_directories(prop_tcs34725 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_tcs34725 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_tcs34725 PRIVATE m)

add_test(NAME tcs34725_prop_tests COMMAND prop_tcs34725)
