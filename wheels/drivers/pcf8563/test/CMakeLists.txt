cmake_minimum_required(VERSION 3.13)
project(test_pcf8563 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_pcf8563
    test_pcf8563.c
    ../pcf8563.c
)

target_include_directories(test_pcf8563 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_pcf8563 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_pcf8563
    test_null_pcf8563.c
    ../pcf8563.c
)
target_include_directories(test_null_pcf8563 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_pcf8563 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_pcf8563
    test_boundary_pcf8563.c
    ../pcf8563.c
)
target_include_directories(test_boundary_pcf8563 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_pcf8563 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME pcf8563_tests COMMAND test_pcf8563)

# hw_model datasheet test
add_executable(test_hw_model_pcf8563
    test_hw_model_pcf8563.c
    ../pcf8563.c
)
target_include_directories(test_hw_model_pcf8563 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_pcf8563 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
add_test(NAME pcf8563_hw_model_tests COMMAND test_hw_model_pcf8563)
add_test(NAME pcf8563_null_tests COMMAND test_null_pcf8563)
add_test(NAME pcf8563_boundary_tests COMMAND test_boundary_pcf8563)
