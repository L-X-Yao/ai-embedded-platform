cmake_minimum_required(VERSION 3.13)
project(test_version_info C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_version_info
    test_version_info.c
    ../version_info.c
)

target_include_directories(test_version_info PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_version_info PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_version_info
    test_null_version_info.c
    ../version_info.c
)
target_include_directories(test_null_version_info PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_version_info PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME version_info_tests COMMAND test_version_info)
add_test(NAME version_info_null_tests COMMAND test_null_version_info)
