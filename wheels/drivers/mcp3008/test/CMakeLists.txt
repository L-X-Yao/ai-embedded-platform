cmake_minimum_required(VERSION 3.13)
project(test_mcp3008 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mcp3008
    test_mcp3008.c
    ../mcp3008.c
)

target_include_directories(test_mcp3008 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_mcp3008 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_mcp3008 PRIVATE m)


add_executable(test_null_mcp3008
    test_null_mcp3008.c
    ../mcp3008.c
)
target_include_directories(test_null_mcp3008 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_mcp3008 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_mcp3008
    test_hw_model_mcp3008.c
    ../mcp3008.c
)
target_include_directories(test_hw_model_mcp3008 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_options(test_hw_model_mcp3008 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_boundary_mcp3008
    test_boundary_mcp3008.c
    ../mcp3008.c
)
target_include_directories(test_boundary_mcp3008 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_mcp3008 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_boundary_mcp3008 PRIVATE m)
enable_testing()
add_test(NAME mcp3008_tests COMMAND test_mcp3008)
add_test(NAME mcp3008_null_tests COMMAND test_null_mcp3008)
add_test(NAME mcp3008_hw_model_tests COMMAND test_hw_model_mcp3008)
add_test(NAME mcp3008_boundary_tests COMMAND test_boundary_mcp3008)
