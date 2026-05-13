cmake_minimum_required(VERSION 3.13)
project(test_dmx512 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_dmx512
    test_dmx512.c
    ../dmx512.c
)

target_include_directories(test_dmx512 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_dmx512 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_dmx512
    test_null_dmx512.c
    ../dmx512.c
)
target_include_directories(test_null_dmx512 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_dmx512 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_boundary_dmx512
    test_boundary_dmx512.c
    ../dmx512.c
)
target_include_directories(test_boundary_dmx512 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_dmx512 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME dmx512_tests COMMAND test_dmx512)
add_test(NAME dmx512_null_tests COMMAND test_null_dmx512)
add_test(NAME dmx512_boundary_tests COMMAND test_boundary_dmx512)
