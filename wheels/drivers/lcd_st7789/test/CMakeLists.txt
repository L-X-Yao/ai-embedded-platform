cmake_minimum_required(VERSION 3.13)
project(test_lcd_st7789 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lcd_st7789
    test_lcd_st7789.c
    ../lcd_st7789.c
)

target_include_directories(test_lcd_st7789 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_lcd_st7789 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_lcd_st7789
    test_hw_model_lcd_st7789.c
    ../lcd_st7789.c
)
target_include_directories(test_hw_model_lcd_st7789 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_lcd_st7789 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)


add_executable(test_null_lcd_st7789
    test_null_lcd_st7789.c
    ../lcd_st7789.c
)
target_include_directories(test_null_lcd_st7789 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_lcd_st7789 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME lcd_st7789_tests COMMAND test_lcd_st7789)
add_test(NAME lcd_st7789_hw_model_tests COMMAND test_hw_model_lcd_st7789)
add_test(NAME lcd_st7789_null_tests COMMAND test_null_lcd_st7789)
