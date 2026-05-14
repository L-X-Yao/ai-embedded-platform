cmake_minimum_required(VERSION 3.13)
project(test_lsm6dso C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lsm6dso
    test_lsm6dso.c
    ../lsm6dso.c
)

target_include_directories(test_lsm6dso PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_lsm6dso PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_lsm6dso PRIVATE CONTRACT_ENABLED)
target_link_libraries(test_lsm6dso PRIVATE m)


add_executable(test_null_lsm6dso
    test_null_lsm6dso.c
    ../lsm6dso.c
)
target_include_directories(test_null_lsm6dso PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_lsm6dso PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_lsm6dso PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_lsm6dso
    test_boundary_lsm6dso.c
    ../lsm6dso.c
)
target_include_directories(test_boundary_lsm6dso PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_boundary_lsm6dso PRIVATE CONTRACT_ENABLED)
target_compile_options(test_boundary_lsm6dso PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME lsm6dso_tests COMMAND test_lsm6dso)

# hw_model datasheet test
add_executable(test_hw_model_lsm6dso
    test_hw_model_lsm6dso.c
    ../lsm6dso.c
)
target_include_directories(test_hw_model_lsm6dso PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_lsm6dso PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_hw_model_lsm6dso PRIVATE CONTRACT_ENABLED)
target_link_libraries(test_hw_model_lsm6dso PRIVATE m)
add_test(NAME lsm6dso_hw_model_tests COMMAND test_hw_model_lsm6dso)
add_test(NAME lsm6dso_null_tests COMMAND test_null_lsm6dso)
add_test(NAME lsm6dso_boundary_tests COMMAND test_boundary_lsm6dso)
