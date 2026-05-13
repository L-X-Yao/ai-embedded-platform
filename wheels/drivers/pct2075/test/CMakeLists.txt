cmake_minimum_required(VERSION 3.13)
project(test_pct2075 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_pct2075
    test_pct2075.c
    ../pct2075.c
)

target_include_directories(test_pct2075 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_pct2075 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_pct2075 PRIVATE CONTRACT_ENABLED)
target_link_libraries(test_pct2075 PRIVATE m)


add_executable(test_null_pct2075
    test_null_pct2075.c
    ../pct2075.c
)
target_include_directories(test_null_pct2075 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_pct2075 PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_pct2075 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME pct2075_tests COMMAND test_pct2075)

# hw_model datasheet test
add_executable(test_hw_model_pct2075
    test_hw_model_pct2075.c
    ../pct2075.c
)
target_include_directories(test_hw_model_pct2075 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_pct2075 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_compile_definitions(test_hw_model_pct2075 PRIVATE CONTRACT_ENABLED)
target_link_libraries(test_hw_model_pct2075 PRIVATE m)
add_test(NAME pct2075_hw_model_tests COMMAND test_hw_model_pct2075)
add_test(NAME pct2075_null_tests COMMAND test_null_pct2075)
