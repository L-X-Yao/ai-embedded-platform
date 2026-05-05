cmake_minimum_required(VERSION 3.13)
project(test_lis3dh C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lis3dh
    test_lis3dh.c
    ../lis3dh.c
)

target_include_directories(test_lis3dh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_lis3dh PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_lis3dh
    prop_lis3dh.c
    ../lis3dh.c
)
target_include_directories(prop_lis3dh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_lis3dh PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)

enable_testing()
add_test(NAME lis3dh_tests COMMAND test_lis3dh)
add_test(NAME lis3dh_prop_tests COMMAND prop_lis3dh)

add_executable(test_hw_model_lis3dh
    test_hw_model_lis3dh.c
    ../lis3dh.c
)
target_include_directories(test_hw_model_lis3dh PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_lis3dh PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_lis3dh PRIVATE m)

add_test(NAME lis3dh_hw_model_tests COMMAND test_hw_model_lis3dh)
