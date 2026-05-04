cmake_minimum_required(VERSION 3.13)
project(test_mahony_ahrs C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mahony_ahrs
    test_mahony_ahrs.c
    ../mahony_ahrs.c
)

target_include_directories(test_mahony_ahrs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mahony_ahrs PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_mahony_ahrs PRIVATE m)

enable_testing()
add_test(NAME mahony_ahrs_tests COMMAND test_mahony_ahrs)
