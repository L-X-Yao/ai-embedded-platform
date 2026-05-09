cmake_minimum_required(VERSION 3.13)
project(test_uuid C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_uuid
    test_uuid.c
    ../uuid.c
)

target_include_directories(test_uuid PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_uuid PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_uuid
    test_null_uuid.c
    ../uuid.c
)
target_include_directories(test_null_uuid PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_uuid PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME uuid_tests COMMAND test_uuid)
add_test(NAME uuid_null_tests COMMAND test_null_uuid)
