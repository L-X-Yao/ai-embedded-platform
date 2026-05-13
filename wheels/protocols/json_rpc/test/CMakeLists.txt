cmake_minimum_required(VERSION 3.13)
project(test_json_rpc C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_json_rpc
    test_json_rpc.c
    ../json_rpc.c
)

target_include_directories(test_json_rpc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_json_rpc PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_json_rpc
    test_null_json_rpc.c
    ../json_rpc.c
)
target_include_directories(test_null_json_rpc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_json_rpc PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_boundary_json_rpc
    test_boundary_json_rpc.c
    ../json_rpc.c
)
target_include_directories(test_boundary_json_rpc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_json_rpc PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME json_rpc_tests COMMAND test_json_rpc)
add_test(NAME json_rpc_null_tests COMMAND test_null_json_rpc)
add_test(NAME json_rpc_boundary_tests COMMAND test_boundary_json_rpc)
