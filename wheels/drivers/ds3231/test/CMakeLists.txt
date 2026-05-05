cmake_minimum_required(VERSION 3.13)
project(test_ds3231 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ds3231
    test_ds3231.c
    ../ds3231.c
)

target_include_directories(test_ds3231 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_ds3231 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME ds3231_tests COMMAND test_ds3231)

add_executable(prop_ds3231
    prop_ds3231.c
    ../ds3231.c
)
target_include_directories(prop_ds3231 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_ds3231 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_ds3231 PRIVATE m)

add_test(NAME ds3231_prop_tests COMMAND prop_ds3231)

add_executable(test_hw_model_ds3231
    test_hw_model_ds3231.c
    ../ds3231.c
)
target_include_directories(test_hw_model_ds3231 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_ds3231 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_ds3231 PRIVATE m)

add_test(NAME ds3231_hw_model_tests COMMAND test_hw_model_ds3231)
