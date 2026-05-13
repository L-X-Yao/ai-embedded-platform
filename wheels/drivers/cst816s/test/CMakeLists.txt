cmake_minimum_required(VERSION 3.13)
project(test_cst816s C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_cst816s
    test_cst816s.c
    ../cst816s.c
)

target_include_directories(test_cst816s PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_cst816s PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_cst816s
    test_null_cst816s.c
    ../cst816s.c
)
target_include_directories(test_null_cst816s PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_cst816s PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_cst816s
    test_boundary_cst816s.c
    ../cst816s.c
)
target_include_directories(test_boundary_cst816s PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_cst816s PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME cst816s_tests COMMAND test_cst816s)

# hw_model datasheet test
add_executable(test_hw_model_cst816s
    test_hw_model_cst816s.c
    ../cst816s.c
)
target_include_directories(test_hw_model_cst816s PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_cst816s PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
add_test(NAME cst816s_hw_model_tests COMMAND test_hw_model_cst816s)
add_test(NAME cst816s_null_tests COMMAND test_null_cst816s)
add_test(NAME cst816s_boundary_tests COMMAND test_boundary_cst816s)
