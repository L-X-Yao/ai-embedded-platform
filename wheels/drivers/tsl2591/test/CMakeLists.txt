cmake_minimum_required(VERSION 3.13)
project(test_tsl2591 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_tsl2591
    test_tsl2591.c
    ../tsl2591.c
)

target_include_directories(test_tsl2591 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_tsl2591 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_tsl2591
    test_null_tsl2591.c
    ../tsl2591.c
)
target_include_directories(test_null_tsl2591 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_tsl2591 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME tsl2591_tests COMMAND test_tsl2591)
add_test(NAME tsl2591_null_tests COMMAND test_null_tsl2591)
