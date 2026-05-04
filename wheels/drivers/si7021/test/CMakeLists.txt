cmake_minimum_required(VERSION 3.13)
project(test_si7021 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_si7021
    test_si7021.c
    ../si7021.c
)

target_include_directories(test_si7021 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_si7021 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_si7021 PRIVATE m)

enable_testing()
add_test(NAME si7021_tests COMMAND test_si7021)

add_executable(prop_si7021
    prop_si7021.c
    ../si7021.c
)
target_include_directories(prop_si7021 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_si7021 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(prop_si7021 PRIVATE m)

add_test(NAME si7021_prop_tests COMMAND prop_si7021)
