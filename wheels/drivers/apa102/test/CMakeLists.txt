cmake_minimum_required(VERSION 3.13)
project(test_apa102 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_apa102
    test_apa102.c
    ../apa102.c
)

target_include_directories(test_apa102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_apa102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_apa102
    test_null_apa102.c
    ../apa102.c
)
target_include_directories(test_null_apa102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_apa102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_apa102
    test_boundary_apa102.c
    ../apa102.c
)
target_include_directories(test_boundary_apa102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_apa102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME apa102_tests COMMAND test_apa102)

# hw_model datasheet test
add_executable(test_hw_model_apa102
    test_hw_model_apa102.c
    ../apa102.c
)
target_include_directories(test_hw_model_apa102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_apa102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
add_test(NAME apa102_hw_model_tests COMMAND test_hw_model_apa102)
add_test(NAME apa102_null_tests COMMAND test_null_apa102)
add_test(NAME apa102_boundary_tests COMMAND test_boundary_apa102)
