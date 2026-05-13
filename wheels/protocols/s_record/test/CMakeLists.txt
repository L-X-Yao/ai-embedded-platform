cmake_minimum_required(VERSION 3.13)
project(test_s_record C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_s_record
    test_s_record.c
    ../s_record.c
)

target_include_directories(test_s_record PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_s_record PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_s_record
    test_null_s_record.c
    ../s_record.c
)
target_include_directories(test_null_s_record PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_s_record PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_s_record
    test_boundary_s_record.c
    ../s_record.c
)
target_include_directories(test_boundary_s_record PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_s_record PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME s_record_tests COMMAND test_s_record)
add_test(NAME s_record_null_tests COMMAND test_null_s_record)
add_test(NAME s_record_boundary_tests COMMAND test_boundary_s_record)
