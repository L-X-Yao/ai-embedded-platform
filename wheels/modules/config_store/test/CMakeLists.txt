cmake_minimum_required(VERSION 3.13)
project(test_config_store C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_config_store
    test_config_store.c
    ../config_store.c
)

target_include_directories(test_config_store PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_compile_options(test_config_store PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_config_store
    test_null_config_store.c
    ../config_store.c
)
target_include_directories(test_null_config_store PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_config_store PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME config_store_tests COMMAND test_config_store)
add_test(NAME config_store_null_tests COMMAND test_null_config_store)
