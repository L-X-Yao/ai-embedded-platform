cmake_minimum_required(VERSION 3.13)
project(test_bitstream C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bitstream
    test_bitstream.c
    ../bitstream.c
)

target_include_directories(test_bitstream PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bitstream PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_bitstream
    test_null_bitstream.c
    ../bitstream.c
)
target_include_directories(test_null_bitstream PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bitstream PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_bitstream
    test_boundary_bitstream.c
    ../bitstream.c
)
target_include_directories(test_boundary_bitstream PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_bitstream PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bitstream_tests COMMAND test_bitstream)
add_test(NAME bitstream_null_tests COMMAND test_null_bitstream)
add_test(NAME bitstream_boundary_tests COMMAND test_boundary_bitstream)
