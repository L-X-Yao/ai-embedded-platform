cmake_minimum_required(VERSION 3.13)
project(test_ms5611 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ms5611
    test_ms5611.c
    ../ms5611.c
)

target_include_directories(test_ms5611 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ms5611 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ms5611 PRIVATE m)


add_executable(test_null_ms5611
    test_null_ms5611.c
    ../ms5611.c
)
target_include_directories(test_null_ms5611 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ms5611 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_ms5611
    test_boundary_ms5611.c
    ../ms5611.c
)
target_include_directories(test_boundary_ms5611 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_ms5611 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_ms5611
    test_hw_model_ms5611.c
    ../ms5611.c
)
target_include_directories(test_hw_model_ms5611 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_ms5611 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
enable_testing()
add_test(NAME ms5611_tests COMMAND test_ms5611)
add_test(NAME ms5611_null_tests COMMAND test_null_ms5611)
add_test(NAME ms5611_boundary_tests COMMAND test_boundary_ms5611)
add_test(NAME ms5611_hw_model_tests COMMAND test_hw_model_ms5611)
