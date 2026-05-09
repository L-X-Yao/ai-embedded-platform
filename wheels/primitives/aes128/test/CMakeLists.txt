cmake_minimum_required(VERSION 3.13)
project(test_aes128 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_aes128
    test_aes128.c
    ../aes128.c
)
target_include_directories(test_aes128 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_aes128 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_aes128
    prop_aes128.c
    ../aes128.c
)
target_include_directories(prop_aes128 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_aes128 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_null_aes128
    test_null_aes128.c
    ../aes128.c
)
target_include_directories(test_null_aes128 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_aes128 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME aes128_tests COMMAND test_aes128)
add_test(NAME aes128_prop  COMMAND prop_aes128)
add_test(NAME aes128_null  COMMAND test_null_aes128)
