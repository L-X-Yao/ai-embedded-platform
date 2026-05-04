cmake_minimum_required(VERSION 3.13)
project(test_data_logger C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_data_logger
    test_data_logger.c
    ../data_logger.c
)

target_include_directories(test_data_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_compile_options(test_data_logger PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_data_logger
    prop_data_logger.c
    ../data_logger.c
)
target_include_directories(prop_data_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_data_logger PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)

enable_testing()
add_test(NAME data_logger_tests COMMAND test_data_logger)
add_test(NAME data_logger_prop_tests COMMAND prop_data_logger)
