cmake_minimum_required(VERSION 3.13)
project(test_htu21d C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_htu21d
    test_htu21d.c
    ../htu21d.c
)

target_include_directories(test_htu21d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_htu21d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_htu21d PRIVATE m)

# hw_model datasheet test
add_executable(test_hw_model_htu21d
    test_hw_model_htu21d.c
    ../htu21d.c
)
target_include_directories(test_hw_model_htu21d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_htu21d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_htu21d PRIVATE m)


add_executable(test_null_htu21d
    test_null_htu21d.c
    ../htu21d.c
)
target_include_directories(test_null_htu21d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_htu21d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_htu21d
    test_boundary_htu21d.c
    ../htu21d.c
)
target_include_directories(test_boundary_htu21d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_htu21d PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME htu21d_tests COMMAND test_htu21d)
add_test(NAME htu21d_hw_model_tests COMMAND test_hw_model_htu21d)
add_test(NAME htu21d_null_tests COMMAND test_null_htu21d)
add_test(NAME htu21d_boundary_tests COMMAND test_boundary_htu21d)
