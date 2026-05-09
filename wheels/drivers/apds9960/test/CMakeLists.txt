cmake_minimum_required(VERSION 3.13)
project(test_apds9960 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_apds9960
    test_apds9960.c
    ../apds9960.c
)

target_include_directories(test_apds9960 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_apds9960 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_apds9960
    test_null_apds9960.c
    ../apds9960.c
)
target_include_directories(test_null_apds9960 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_apds9960 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME apds9960_tests COMMAND test_apds9960)

add_executable(prop_apds9960
    prop_apds9960.c
    ../apds9960.c
)
target_include_directories(prop_apds9960 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_apds9960 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME apds9960_prop_tests COMMAND prop_apds9960)

add_executable(test_hw_model_apds9960
    test_hw_model_apds9960.c
    ../apds9960.c
)
target_include_directories(test_hw_model_apds9960 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_apds9960 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_apds9960 PRIVATE m)

add_test(NAME apds9960_hw_model_tests COMMAND test_hw_model_apds9960)
add_test(NAME apds9960_null_tests COMMAND test_null_apds9960)
