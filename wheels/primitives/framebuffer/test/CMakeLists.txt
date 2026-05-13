cmake_minimum_required(VERSION 3.13)
project(test_framebuffer C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_framebuffer
    test_framebuffer.c
    ../framebuffer.c
)

target_include_directories(test_framebuffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_framebuffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_framebuffer
    test_null_framebuffer.c
    ../framebuffer.c
)
target_include_directories(test_null_framebuffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_framebuffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_framebuffer
    test_boundary_framebuffer.c
    ../framebuffer.c
)
target_include_directories(test_boundary_framebuffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_framebuffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME framebuffer_tests COMMAND test_framebuffer)
add_test(NAME framebuffer_null_tests COMMAND test_null_framebuffer)
add_test(NAME framebuffer_boundary_tests COMMAND test_boundary_framebuffer)
