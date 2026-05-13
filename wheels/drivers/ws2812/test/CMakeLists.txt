cmake_minimum_required(VERSION 3.13)
project(test_ws2812 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ws2812
    test_ws2812.c
    ../ws2812.c
)

target_include_directories(test_ws2812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)

target_compile_options(test_ws2812 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_ws2812
    test_null_ws2812.c
    ../ws2812.c
)
target_include_directories(test_null_ws2812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ws2812 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_ws2812
    test_boundary_ws2812.c
    ../ws2812.c
)
target_include_directories(test_boundary_ws2812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_ws2812 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ws2812_tests COMMAND test_ws2812)

add_executable(prop_ws2812
    prop_ws2812.c
    ../ws2812.c
)
target_include_directories(prop_ws2812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_ws2812 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME ws2812_prop_tests COMMAND prop_ws2812)

add_executable(test_hw_model_ws2812
    test_hw_model_ws2812.c
    ../ws2812.c
)
target_include_directories(test_hw_model_ws2812 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_ws2812 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME ws2812_hw_model_tests COMMAND test_hw_model_ws2812)
add_test(NAME ws2812_null_tests COMMAND test_null_ws2812)
add_test(NAME ws2812_boundary_tests COMMAND test_boundary_ws2812)
