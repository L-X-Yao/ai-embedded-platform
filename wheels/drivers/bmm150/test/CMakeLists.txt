cmake_minimum_required(VERSION 3.13)
project(test_bmm150 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bmm150
    test_bmm150.c
    ../bmm150.c
)

target_include_directories(test_bmm150 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bmm150 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME bmm150_tests COMMAND test_bmm150)

# hw_model datasheet test
add_executable(test_hw_model_bmm150
    test_hw_model_bmm150.c
    ../bmm150.c
)
target_include_directories(test_hw_model_bmm150 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_bmm150 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME bmm150_hw_model_tests COMMAND test_hw_model_bmm150)
