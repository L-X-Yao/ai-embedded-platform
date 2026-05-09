cmake_minimum_required(VERSION 3.13)
project(test_gnss_filter C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_gnss_filter
    test_gnss_filter.c
    ../gnss_filter.c
)

target_include_directories(test_gnss_filter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/test always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_gnss_filter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_gnss_filter
    prop_gnss_filter.c
    ../gnss_filter.c
)
target_include_directories(prop_gnss_filter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_gnss_filter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(prop_gnss_filter PRIVATE m)


add_executable(test_null_gnss_filter
    test_null_gnss_filter.c
    ../gnss_filter.c
)
target_include_directories(test_null_gnss_filter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_gnss_filter PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME gnss_filter_tests COMMAND test_gnss_filter)
add_test(NAME gnss_filter_props COMMAND prop_gnss_filter)
add_test(NAME gnss_filter_null_tests COMMAND test_null_gnss_filter)
