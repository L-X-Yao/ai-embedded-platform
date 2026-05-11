cmake_minimum_required(VERSION 3.13)
project(test_ina226 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ina226
    test_ina226.c
    ../ina226.c
)

target_include_directories(test_ina226 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ina226 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

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
target_compile_options(test_hw_model_ina226 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME ina226_hw_model_tests COMMAND test_hw_model_ina226)
