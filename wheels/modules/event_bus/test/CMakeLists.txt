cmake_minimum_required(VERSION 3.13)
project(test_event_bus C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_event_bus test_event_bus.c ../event_bus.c)
target_include_directories(test_event_bus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_event_bus PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_event_bus
    test_null_event_bus.c
    ../event_bus.c
)
target_include_directories(test_null_event_bus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_event_bus PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_event_bus
    test_boundary_event_bus.c
    ../event_bus.c
)
target_include_directories(test_boundary_event_bus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_event_bus PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME event_bus_tests COMMAND test_event_bus)
add_test(NAME event_bus_null_tests COMMAND test_null_event_bus)
add_test(NAME event_bus_boundary_tests COMMAND test_boundary_event_bus)
