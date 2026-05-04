cmake_minimum_required(VERSION 3.13)
project(test_tca9548a C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_tca9548a
    test_tca9548a.c
    ../tca9548a.c
)

target_include_directories(test_tca9548a PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_tca9548a PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME tca9548a_tests COMMAND test_tca9548a)

add_executable(prop_tca9548a
    prop_tca9548a.c
    ../tca9548a.c
)
target_include_directories(prop_tca9548a PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_tca9548a PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME tca9548a_prop_tests COMMAND prop_tca9548a)
