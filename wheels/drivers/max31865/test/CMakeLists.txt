cmake_minimum_required(VERSION 3.13)
project(test_max31865 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_max31865
    test_max31865.c
    ../max31865.c
)

target_include_directories(test_max31865 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_max31865 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_max31865
    test_hw_model_max31865.c
    ../max31865.c
)
target_include_directories(test_hw_model_max31865 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_max31865 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_max31865 PRIVATE m)

enable_testing()
add_test(NAME max31865_tests COMMAND test_max31865)
add_test(NAME max31865_hw_model_tests COMMAND test_hw_model_max31865)

add_executable(prop_max31865
    prop_max31865.c
    ../max31865.c
)
target_include_directories(prop_max31865 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_max31865 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_max31865 PRIVATE m)

add_test(NAME max31865_prop_tests COMMAND prop_max31865)
