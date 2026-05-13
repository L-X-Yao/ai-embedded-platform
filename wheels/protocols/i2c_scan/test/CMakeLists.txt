cmake_minimum_required(VERSION 3.13)
project(test_i2c_scan C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_i2c_scan
    test_i2c_scan.c
    ../i2c_scan.c
)

target_include_directories(test_i2c_scan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_i2c_scan PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_i2c_scan
    test_null_i2c_scan.c
    ../i2c_scan.c
)
target_include_directories(test_null_i2c_scan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_i2c_scan PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_i2c_scan
    test_boundary_i2c_scan.c
    ../i2c_scan.c
)
target_include_directories(test_boundary_i2c_scan PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_i2c_scan PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME i2c_scan_tests COMMAND test_i2c_scan)
add_test(NAME i2c_scan_null_tests COMMAND test_null_i2c_scan)
add_test(NAME i2c_scan_boundary_tests COMMAND test_boundary_i2c_scan)
