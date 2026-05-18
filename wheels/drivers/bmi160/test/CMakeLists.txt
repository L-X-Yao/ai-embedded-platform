cmake_minimum_required(VERSION 3.13)
project(test_bmi160 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bmi160
    test_bmi160.c
    ../bmi160.c
)

target_include_directories(test_bmi160 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bmi160 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_bmi160 PRIVATE m)


add_executable(test_null_bmi160
    test_null_bmi160.c
    ../bmi160.c
)
target_include_directories(test_null_bmi160 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bmi160 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_bmi160
    test_boundary_bmi160.c
    ../bmi160.c
)
target_include_directories(test_boundary_bmi160 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_bmi160 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bmi160_tests COMMAND test_bmi160)

# hw_model datasheet test
add_executable(test_hw_model_bmi160
    test_hw_model_bmi160.c
    ../bmi160.c
)
target_include_directories(test_hw_model_bmi160 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_bmi160 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_bmi160 PRIVATE m)

add_test(NAME bmi160_hw_model_tests COMMAND test_hw_model_bmi160)
add_test(NAME bmi160_null_tests COMMAND test_null_bmi160)
add_test(NAME bmi160_boundary_tests COMMAND test_boundary_bmi160)
