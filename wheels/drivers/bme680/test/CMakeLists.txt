cmake_minimum_required(VERSION 3.13)
project(test_bme680 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bme680
    test_bme680.c
    ../bme680.c
)

target_include_directories(test_bme680 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_bme680 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME bme680_tests COMMAND test_bme680)

add_executable(prop_bme680
    prop_bme680.c
    ../bme680.c
)
target_include_directories(prop_bme680 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_bme680 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME bme680_prop_tests COMMAND prop_bme680)
