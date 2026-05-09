cmake_minimum_required(VERSION 3.13)
project(test_aht20 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_aht20
    test_aht20.c
    ../aht20.c
)

target_include_directories(test_aht20 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_aht20 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_aht20 PRIVATE m)

add_executable(test_null_aht20
    test_null_aht20.c
    ../aht20.c
)
target_include_directories(test_null_aht20 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_aht20 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_null_aht20 PRIVATE m)

enable_testing()
add_test(NAME aht20_tests COMMAND test_aht20)
add_test(NAME aht20_null_tests COMMAND test_null_aht20)
