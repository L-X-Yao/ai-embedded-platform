cmake_minimum_required(VERSION 3.13)
project(test_ltr390 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(HARNESS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness)

add_executable(test_ltr390
    test_ltr390.c
    ../ltr390.c
)

target_include_directories(test_ltr390 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HARNESS_DIR}
)
target_compile_options(test_ltr390 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_ltr390 PRIVATE CONTRACT_ENABLED)


add_executable(test_null_ltr390
    test_null_ltr390.c
    ../ltr390.c
)
target_include_directories(test_null_ltr390 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_ltr390 PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_ltr390 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ltr390_tests COMMAND test_ltr390)

# hw_model datasheet test
add_executable(test_hw_model_ltr390
    test_hw_model_ltr390.c
    ../ltr390.c
)
target_include_directories(test_hw_model_ltr390 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${HARNESS_DIR}
)
target_compile_options(test_hw_model_ltr390 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_hw_model_ltr390 PRIVATE CONTRACT_ENABLED)
add_test(NAME ltr390_hw_model_tests COMMAND test_hw_model_ltr390)
add_test(NAME ltr390_null_tests COMMAND test_null_ltr390)
