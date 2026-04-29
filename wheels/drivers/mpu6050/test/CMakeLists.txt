cmake_minimum_required(VERSION 3.13)
project(test_mpu6050 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mpu6050
    test_mpu6050.c
    ../mpu6050.c
)

target_include_directories(test_mpu6050 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_compile_options(test_mpu6050 PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(test_mpu6050 PRIVATE m)

enable_testing()
add_test(NAME mpu6050_tests COMMAND test_mpu6050)
