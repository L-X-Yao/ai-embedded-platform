cmake_minimum_required(VERSION 3.13)
project(test_ota_manager C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ota_manager
    test_ota_manager.c
    ../ota_manager.c
)

target_include_directories(test_ota_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_compile_options(test_ota_manager PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ota_manager_tests COMMAND test_ota_manager)
