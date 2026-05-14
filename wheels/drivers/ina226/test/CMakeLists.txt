cmake_minimum_required(VERSION 3.13)
project(test_ina226 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ina226
    test_ina226.c
    ../ina226.c
)

target_include_directories(test_ina226 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ina226 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ina226 PRIVATE m)


add_executable(test_null_ina226
    test_null_ina226.c
    ../ina226.c
)
target_include_directories(test_null_ina226 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ina226 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_ina226
    test_boundary_ina226.c
    ../ina226.c
)
target_include_directories(test_boundary_ina226 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_ina226 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ina226_tests COMMAND test_ina226)

# hw_model datasheet test
add_executable(test_hw_model_ina226
    test_hw_model_ina226.c
    ../ina226.c
)
target_include_directories(test_hw_model_ina226 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_ina226 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_ina226 PRIVATE m)
add_test(NAME ina226_hw_model_tests COMMAND test_hw_model_ina226)
add_test(NAME ina226_null_tests COMMAND test_null_ina226)
add_test(NAME ina226_boundary_tests COMMAND test_boundary_ina226)
