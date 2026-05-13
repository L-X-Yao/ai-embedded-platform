cmake_minimum_required(VERSION 3.13)
project(test_sensor_hub C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sensor_hub
    test_sensor_hub.c
    ../sensor_hub.c
)

target_include_directories(test_sensor_hub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_sensor_hub PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_sensor_hub
    prop_sensor_hub.c
    ../sensor_hub.c
)
target_include_directories(prop_sensor_hub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_sensor_hub PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)


add_executable(test_null_sensor_hub
    test_null_sensor_hub.c
    ../sensor_hub.c
)
target_include_directories(test_null_sensor_hub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_sensor_hub PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_sensor_hub
    test_boundary_sensor_hub.c
    ../sensor_hub.c
)
target_include_directories(test_boundary_sensor_hub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_sensor_hub PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME sensor_hub_tests COMMAND test_sensor_hub)
add_test(NAME sensor_hub_prop_tests COMMAND prop_sensor_hub)
add_test(NAME sensor_hub_null_tests COMMAND test_null_sensor_hub)
add_test(NAME sensor_hub_boundary_tests COMMAND test_boundary_sensor_hub)
