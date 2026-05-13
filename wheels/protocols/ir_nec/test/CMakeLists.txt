cmake_minimum_required(VERSION 3.13)
project(test_ir_nec C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ir_nec
    test_ir_nec.c
    ../ir_nec.c
)

target_include_directories(test_ir_nec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_ir_nec PRIVATE CONTRACT_ENABLED)
target_compile_options(test_ir_nec PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_ir_nec
    test_null_ir_nec.c
    ../ir_nec.c
)
target_include_directories(test_null_ir_nec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_ir_nec PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_ir_nec PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_ir_nec
    test_boundary_ir_nec.c
    ../ir_nec.c
)
target_include_directories(test_boundary_ir_nec PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_boundary_ir_nec PRIVATE CONTRACT_ENABLED)
target_compile_options(test_boundary_ir_nec PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ir_nec_tests COMMAND test_ir_nec)
add_test(NAME ir_nec_null_tests COMMAND test_null_ir_nec)
add_test(NAME ir_nec_boundary_tests COMMAND test_boundary_ir_nec)
