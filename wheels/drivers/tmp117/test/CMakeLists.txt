cmake_minimum_required(VERSION 3.13)
project(test_tmp117 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_tmp117
    test_tmp117.c
    ../tmp117.c
)

target_include_directories(test_tmp117 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_tmp117 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_tmp117
    test_null_tmp117.c
    ../tmp117.c
)
target_include_directories(test_null_tmp117 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_tmp117 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_tmp117
    test_boundary_tmp117.c
    ../tmp117.c
)
target_include_directories(test_boundary_tmp117 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_tmp117 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME tmp117_tests COMMAND test_tmp117)

# hw_model datasheet test
add_executable(test_hw_model_tmp117
    test_hw_model_tmp117.c
    ../tmp117.c
)
target_include_directories(test_hw_model_tmp117 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_tmp117 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME tmp117_hw_model_tests COMMAND test_hw_model_tmp117)
add_test(NAME tmp117_null_tests COMMAND test_null_tmp117)
add_test(NAME tmp117_boundary_tests COMMAND test_boundary_tmp117)
