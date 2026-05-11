cmake_minimum_required(VERSION 3.13)
project(test_ccs811 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ccs811
    test_ccs811.c
    ../ccs811.c
)

target_include_directories(test_ccs811 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ccs811 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ccs811 PRIVATE m)


add_executable(test_null_ccs811
    test_null_ccs811.c
    ../ccs811.c
)
target_include_directories(test_null_ccs811 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ccs811 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ccs811_tests COMMAND test_ccs811)
add_test(NAME ccs811_null_tests COMMAND test_null_ccs811)
