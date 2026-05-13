cmake_minimum_required(VERSION 3.13)
project(test_varint C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_varint
    test_varint.c
    ../varint.c
)

target_include_directories(test_varint PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_varint PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_varint
    prop_varint.c
    ../varint.c
)
target_include_directories(prop_varint PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_varint PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_null_varint
    test_null_varint.c
    ../varint.c
)
target_include_directories(test_null_varint PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_varint PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_varint
    test_boundary_varint.c
    ../varint.c
)
target_include_directories(test_boundary_varint PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_varint PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME varint_tests COMMAND test_varint)
add_test(NAME varint_prop  COMMAND prop_varint)
add_test(NAME varint_null_tests COMMAND test_null_varint)
add_test(NAME varint_boundary_tests COMMAND test_boundary_varint)
