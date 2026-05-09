cmake_minimum_required(VERSION 3.13)
project(test_power_manager C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_power_manager
    test_power_manager.c
    ../power_manager.c
)

target_include_directories(test_power_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_power_manager PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_power_manager
    test_null_power_manager.c
    ../power_manager.c
)
target_include_directories(test_null_power_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_power_manager PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME power_manager_tests COMMAND test_power_manager)
add_test(NAME power_manager_null_tests COMMAND test_null_power_manager)
