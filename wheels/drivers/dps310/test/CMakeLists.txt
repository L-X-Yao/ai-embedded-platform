cmake_minimum_required(VERSION 3.13)
project(test_dps310 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_dps310
    test_dps310.c
    ../dps310.c
)

target_include_directories(test_dps310 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_dps310 PRIVATE CONTRACT_ENABLED)
target_compile_options(test_dps310 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_dps310 PRIVATE m)


add_executable(test_null_dps310
    test_null_dps310.c
    ../dps310.c
)
target_include_directories(test_null_dps310 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_dps310 PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_dps310 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_null_dps310 PRIVATE m)

enable_testing()
add_test(NAME dps310_tests COMMAND test_dps310)

# hw_model datasheet test
add_executable(test_hw_model_dps310
    test_hw_model_dps310.c
    ../dps310.c
)
target_include_directories(test_hw_model_dps310 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_hw_model_dps310 PRIVATE CONTRACT_ENABLED)
target_compile_options(test_hw_model_dps310 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_hw_model_dps310 PRIVATE m)
add_test(NAME dps310_hw_model_tests COMMAND test_hw_model_dps310)
add_test(NAME dps310_null_tests COMMAND test_null_dps310)
