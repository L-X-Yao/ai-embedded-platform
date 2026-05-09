cmake_minimum_required(VERSION 3.13)
project(test_air780eg C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_air780eg
    test_air780eg.c
    ../air780eg.c
)

target_include_directories(test_air780eg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_air780eg PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_null_air780eg
    test_null_air780eg.c
    ../air780eg.c
)
target_include_directories(test_null_air780eg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_air780eg PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# Executable name is test_air780eg_props so that rglob on air780eg.c.gcda
# finds test_air780eg.dir before test_air780eg_props.dir alphabetically,
# ensuring audit_coverage.py reports coverage from the main test binary.
add_executable(test_air780eg_props
    prop_air780eg.c
    ../air780eg.c
)
target_include_directories(test_air780eg_props PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_air780eg_props PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME air780eg_tests COMMAND test_air780eg)
add_test(NAME air780eg_null  COMMAND test_null_air780eg)
add_test(NAME air780eg_props COMMAND test_air780eg_props)
