cmake_minimum_required(VERSION 3.13)
project(test_servo C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_servo test_servo.c ../servo.c)
target_include_directories(test_servo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_servo PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME servo_tests COMMAND test_servo)

add_executable(prop_servo
    prop_servo.c
    ../servo.c
)
target_include_directories(prop_servo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_servo PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME servo_prop_tests COMMAND prop_servo)
