cmake_minimum_required(VERSION 3.13)
project(test_as5600 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_as5600
    test_as5600.c
    ../as5600.c
)

target_include_directories(test_as5600 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_as5600 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_as5600
    test_hw_model_as5600.c
    ../as5600.c
)
target_include_directories(test_hw_model_as5600 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_as5600 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)


add_executable(test_null_as5600
    test_null_as5600.c
    ../as5600.c
)
target_include_directories(test_null_as5600 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_as5600 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_as5600
    test_boundary_as5600.c
    ../as5600.c
)
target_include_directories(test_boundary_as5600 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_as5600 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME as5600_tests COMMAND test_as5600)
add_test(NAME as5600_hw_model_tests COMMAND test_hw_model_as5600)

add_executable(prop_as5600
    prop_as5600.c
    ../as5600.c
)
target_include_directories(prop_as5600 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_as5600 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME as5600_prop_tests COMMAND prop_as5600)
add_test(NAME as5600_null_tests COMMAND test_null_as5600)
add_test(NAME as5600_boundary_tests COMMAND test_boundary_as5600)
