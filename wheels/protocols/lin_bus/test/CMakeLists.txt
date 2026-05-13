cmake_minimum_required(VERSION 3.13)
project(test_lin_bus C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lin_bus
    test_lin_bus.c
    ../lin_bus.c
)

target_include_directories(test_lin_bus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_lin_bus PRIVATE CONTRACT_ENABLED)
target_compile_options(test_lin_bus PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_lin_bus
    test_null_lin_bus.c
    ../lin_bus.c
)
target_include_directories(test_null_lin_bus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_lin_bus PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_lin_bus PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME lin_bus_tests COMMAND test_lin_bus)
add_test(NAME lin_bus_null_tests COMMAND test_null_lin_bus)
