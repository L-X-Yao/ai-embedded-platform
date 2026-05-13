cmake_minimum_required(VERSION 3.13)
project(test_scale_map C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_scale_map
    test_scale_map.c
    ../scale_map.c
)

target_include_directories(test_scale_map PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_scale_map PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_null_scale_map
    test_null_scale_map.c
    ../scale_map.c
)
target_include_directories(test_null_scale_map PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_scale_map PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_scale_map
    test_boundary_scale_map.c
    ../scale_map.c
)
target_include_directories(test_boundary_scale_map PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_scale_map PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_boundary_scale_map PRIVATE m)
enable_testing()
add_test(NAME scale_map_tests COMMAND test_scale_map)
add_test(NAME scale_map_null_tests COMMAND test_null_scale_map)
add_test(NAME scale_map_boundary_tests COMMAND test_boundary_scale_map)
