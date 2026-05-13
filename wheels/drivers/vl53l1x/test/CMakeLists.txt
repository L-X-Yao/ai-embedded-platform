cmake_minimum_required(VERSION 3.13)
project(test_vl53l1x C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_vl53l1x
    test_vl53l1x.c
    ../vl53l1x.c
)

target_include_directories(test_vl53l1x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_vl53l1x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_vl53l1x
    test_null_vl53l1x.c
    ../vl53l1x.c
)
target_include_directories(test_null_vl53l1x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_vl53l1x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME vl53l1x_tests COMMAND test_vl53l1x)

# hw_model datasheet test
add_executable(test_hw_model_vl53l1x
    test_hw_model_vl53l1x.c
    ../vl53l1x.c
)
target_include_directories(test_hw_model_vl53l1x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_vl53l1x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME vl53l1x_hw_model_tests COMMAND test_hw_model_vl53l1x)
add_test(NAME vl53l1x_null_tests COMMAND test_null_vl53l1x)
