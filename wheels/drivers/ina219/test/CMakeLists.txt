cmake_minimum_required(VERSION 3.13)
project(test_ina219 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ina219
    test_ina219.c
    ../ina219.c
)

target_include_directories(test_ina219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ina219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ina219 PRIVATE m)


add_executable(test_null_ina219
    test_null_ina219.c
    ../ina219.c
)
target_include_directories(test_null_ina219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ina219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ina219_tests COMMAND test_ina219)

add_executable(prop_ina219
    prop_ina219.c
    ../ina219.c
)
target_include_directories(prop_ina219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_ina219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_ina219 PRIVATE m)

add_test(NAME ina219_prop_tests COMMAND prop_ina219)

add_executable(test_hw_model_ina219
    test_hw_model_ina219.c
    ../ina219.c
)
target_include_directories(test_hw_model_ina219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_ina219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_ina219 PRIVATE m)

add_test(NAME ina219_hw_model_tests COMMAND test_hw_model_ina219)
add_test(NAME ina219_null_tests COMMAND test_null_ina219)
