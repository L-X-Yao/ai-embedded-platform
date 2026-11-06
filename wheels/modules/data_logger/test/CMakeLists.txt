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
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)

target_compile_options(test_data_logger PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_data_logger
    prop_data_logger.c
    ../data_logger.c
)
target_include_directories(prop_data_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_data_logger PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)


add_executable(test_null_data_logger
    test_null_data_logger.c
    ../data_logger.c
)
target_include_directories(test_null_data_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_data_logger PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_data_logger
    test_boundary_data_logger.c
    ../data_logger.c
)
target_include_directories(test_boundary_data_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_data_logger PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_boundary_data_logger PRIVATE m)

# Includes data_logger.c itself, to build it against a snprintf without floats.
add_executable(test_nofloat_data_logger
    test_nofloat_data_logger.c
)
target_include_directories(test_nofloat_data_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_nofloat_data_logger PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_nofloat_data_logger PRIVATE m)
enable_testing()
add_test(NAME data_logger_tests COMMAND test_data_logger)
add_test(NAME data_logger_prop_tests COMMAND prop_data_logger)
add_test(NAME data_logger_null_tests COMMAND test_null_data_logger)
add_test(NAME data_logger_boundary_tests COMMAND test_boundary_data_logger)
add_test(NAME data_logger_nofloat_tests COMMAND test_nofloat_data_logger)
