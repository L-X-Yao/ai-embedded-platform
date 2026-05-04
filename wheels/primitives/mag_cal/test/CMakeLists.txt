cmake_minimum_required(VERSION 3.13)
project(test_mag_cal C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mag_cal
    test_mag_cal.c
    ../mag_cal.c
)

target_include_directories(test_mag_cal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mag_cal PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_mag_cal PRIVATE m)

enable_testing()
add_test(NAME mag_cal_tests COMMAND test_mag_cal)
