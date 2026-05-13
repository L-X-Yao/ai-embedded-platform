cmake_minimum_required(VERSION 3.13)
project(test_ina3221 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ina3221
    test_ina3221.c
    ../ina3221.c
)
target_include_directories(test_ina3221 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ina3221 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ina3221 PRIVATE m)

add_executable(test_hw_model_ina3221
    test_hw_model_ina3221.c
    ../ina3221.c
)
target_include_directories(test_hw_model_ina3221 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_ina3221 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_ina3221 PRIVATE m)


add_executable(test_boundary_ina3221
    test_boundary_ina3221.c
    ../ina3221.c
)
target_include_directories(test_boundary_ina3221 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_ina3221 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ina3221_tests COMMAND test_ina3221)
add_test(NAME ina3221_hw_model_tests COMMAND test_hw_model_ina3221)
add_test(NAME ina3221_boundary_tests COMMAND test_boundary_ina3221)
