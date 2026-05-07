cmake_minimum_required(VERSION 3.13)
project(test_test_dummy2 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_test_dummy2
    test_test_dummy2.c
    ../test_dummy2.c
)

target_include_directories(test_test_dummy2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/test always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_test_dummy2 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME test_dummy2_tests COMMAND test_test_dummy2)

# hw_model datasheet test
add_executable(test_hw_model_test_dummy2
    test_hw_model_test_dummy2.c
    ../test_dummy2.c
)
target_include_directories(test_hw_model_test_dummy2 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_test_dummy2 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME test_dummy2_hw_model_tests COMMAND test_hw_model_test_dummy2)
