cmake_minimum_required(VERSION 3.13)
project(test_soft_spi C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_soft_spi
    test_soft_spi.c
    ../soft_spi.c
)

target_include_directories(test_soft_spi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/test always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_soft_spi PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_soft_spi
    prop_soft_spi.c
    ../soft_spi.c
)
target_include_directories(prop_soft_spi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_soft_spi PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_null_soft_spi
    test_null_soft_spi.c
    ../soft_spi.c
)
target_include_directories(test_null_soft_spi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_soft_spi PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME soft_spi_tests COMMAND test_soft_spi)
add_test(NAME soft_spi_props COMMAND prop_soft_spi)
add_test(NAME soft_spi_null  COMMAND test_null_soft_spi)
