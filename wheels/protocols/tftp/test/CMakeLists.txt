cmake_minimum_required(VERSION 3.13)
project(test_tftp C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_tftp
    test_tftp.c
    ../tftp.c
)

target_include_directories(test_tftp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_tftp PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_tftp
    test_null_tftp.c
    ../tftp.c
)
target_include_directories(test_null_tftp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_tftp PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_tftp
    test_boundary_tftp.c
    ../tftp.c
)
target_include_directories(test_boundary_tftp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_tftp PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME tftp_tests COMMAND test_tftp)
add_test(NAME tftp_null_tests COMMAND test_null_tftp)
add_test(NAME tftp_boundary_tests COMMAND test_boundary_tftp)
