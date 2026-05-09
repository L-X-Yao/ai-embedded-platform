cmake_minimum_required(VERSION 3.13)
project(test_sha256 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sha256
    test_sha256.c
    ../sha256.c
)
target_include_directories(test_sha256 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_sha256 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_sha256
    prop_sha256.c
    ../sha256.c
)
target_include_directories(prop_sha256 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_sha256 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_null_sha256
    test_null_sha256.c
    ../sha256.c
)
target_include_directories(test_null_sha256 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_sha256 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME sha256_tests COMMAND test_sha256)
add_test(NAME sha256_prop  COMMAND prop_sha256)
add_test(NAME sha256_null  COMMAND test_null_sha256)
