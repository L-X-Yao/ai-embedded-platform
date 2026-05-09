cmake_minimum_required(VERSION 3.13)
project(test_xgzp6816d C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_xgzp6816d
    test_xgzp6816d.c
    ../xgzp6816d.c
)

target_include_directories(test_xgzp6816d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_link_libraries(test_xgzp6816d PRIVATE m)
target_compile_options(test_xgzp6816d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_xgzp6816d
    test_hw_model_xgzp6816d.c
    ../xgzp6816d.c
)
target_include_directories(test_hw_model_xgzp6816d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_xgzp6816d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_xgzp6816d PRIVATE m)


add_executable(test_null_xgzp6816d
    test_null_xgzp6816d.c
    ../xgzp6816d.c
)
target_include_directories(test_null_xgzp6816d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_xgzp6816d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME xgzp6816d_tests COMMAND test_xgzp6816d)
add_test(NAME xgzp6816d_hw_model_tests COMMAND test_hw_model_xgzp6816d)

add_executable(prop_xgzp6816d
    prop_xgzp6816d.c
    ../xgzp6816d.c
)
target_include_directories(prop_xgzp6816d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_xgzp6816d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_xgzp6816d PRIVATE m)

add_test(NAME xgzp6816d_prop_tests COMMAND prop_xgzp6816d)
add_test(NAME xgzp6816d_null_tests COMMAND test_null_xgzp6816d)
