cmake_minimum_required(VERSION 3.13)
project(test_hysteresis C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hysteresis
    test_hysteresis.c
    ../hysteresis.c
)

target_include_directories(test_hysteresis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hysteresis PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_hysteresis PRIVATE CONTRACT_ENABLED)
target_link_libraries(test_hysteresis PRIVATE m)


add_executable(test_null_hysteresis
    test_null_hysteresis.c
    ../hysteresis.c
)
target_include_directories(test_null_hysteresis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_hysteresis PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_hysteresis PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME hysteresis_tests COMMAND test_hysteresis)
add_test(NAME hysteresis_null_tests COMMAND test_null_hysteresis)
