cmake_minimum_required(VERSION 3.13)
project(test_canbus C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_canbus
    test_canbus.c
    ../canbus.c
)

target_include_directories(test_canbus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_canbus PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_canbus
    test_null_canbus.c
    ../canbus.c
)
target_include_directories(test_null_canbus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_canbus PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_canbus
    test_boundary_canbus.c
    ../canbus.c
)
target_include_directories(test_boundary_canbus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_canbus PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME canbus_tests COMMAND test_canbus)
add_test(NAME canbus_null_tests COMMAND test_null_canbus)
add_test(NAME canbus_boundary_tests COMMAND test_boundary_canbus)
