cmake_minimum_required(VERSION 3.13)
project(test_telemetry C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_telemetry
    test_telemetry.c
    ../telemetry.c
)

target_include_directories(test_telemetry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)

target_compile_options(test_telemetry PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_telemetry PRIVATE m)


add_executable(test_null_telemetry
    test_null_telemetry.c
    ../telemetry.c
)
target_include_directories(test_null_telemetry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_telemetry PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_telemetry
    test_boundary_telemetry.c
    ../telemetry.c
)
target_include_directories(test_boundary_telemetry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_telemetry PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_boundary_telemetry PRIVATE m)

# Includes telemetry.c itself, to build it against a snprintf without floats.
add_executable(test_nofloat_telemetry
    test_nofloat_telemetry.c
)
target_include_directories(test_nofloat_telemetry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_nofloat_telemetry PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_nofloat_telemetry PRIVATE m)
enable_testing()
add_test(NAME telemetry_tests COMMAND test_telemetry)
add_test(NAME telemetry_null_tests COMMAND test_null_telemetry)
add_test(NAME telemetry_boundary_tests COMMAND test_boundary_telemetry)
add_test(NAME telemetry_nofloat_tests COMMAND test_nofloat_telemetry)
