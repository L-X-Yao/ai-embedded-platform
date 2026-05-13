cmake_minimum_required(VERSION 3.13)
project(test_hdlc C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hdlc
    test_hdlc.c
    ../hdlc.c
)

target_include_directories(test_hdlc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_hdlc PRIVATE CONTRACT_ENABLED)
target_compile_options(test_hdlc PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_hdlc
    test_null_hdlc.c
    ../hdlc.c
)
target_include_directories(test_null_hdlc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_hdlc PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_hdlc PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_hdlc
    test_boundary_hdlc.c
    ../hdlc.c
)
target_include_directories(test_boundary_hdlc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_boundary_hdlc PRIVATE CONTRACT_ENABLED)
target_compile_options(test_boundary_hdlc PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME hdlc_tests COMMAND test_hdlc)
add_test(NAME hdlc_null_tests COMMAND test_null_hdlc)
add_test(NAME hdlc_boundary_tests COMMAND test_boundary_hdlc)
