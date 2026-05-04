cmake_minimum_required(VERSION 3.13)
project(test_ds18b20 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ds18b20
    test_ds18b20.c
    ../ds18b20.c
)
target_include_directories(test_ds18b20 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_ds18b20 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ds18b20_tests COMMAND test_ds18b20)

add_executable(prop_ds18b20
    prop_ds18b20.c
    ../ds18b20.c
)
target_include_directories(prop_ds18b20 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_ds18b20 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME ds18b20_prop_tests COMMAND prop_ds18b20)
