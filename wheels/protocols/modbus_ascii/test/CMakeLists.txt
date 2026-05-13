cmake_minimum_required(VERSION 3.13)
project(test_modbus_ascii C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_modbus_ascii
    test_modbus_ascii.c
    ../modbus_ascii.c
)

target_include_directories(test_modbus_ascii PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_modbus_ascii PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_modbus_ascii
    test_null_modbus_ascii.c
    ../modbus_ascii.c
)
target_include_directories(test_null_modbus_ascii PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_modbus_ascii PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME modbus_ascii_tests COMMAND test_modbus_ascii)
add_test(NAME modbus_ascii_null_tests COMMAND test_null_modbus_ascii)
