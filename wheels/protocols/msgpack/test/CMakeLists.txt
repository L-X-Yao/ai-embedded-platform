cmake_minimum_required(VERSION 3.13)
project(test_msgpack C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_msgpack
    test_msgpack.c
    ../msgpack.c
)

target_include_directories(test_msgpack PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_msgpack PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_msgpack
    test_null_msgpack.c
    ../msgpack.c
)
target_include_directories(test_null_msgpack PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_msgpack PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_msgpack
    test_boundary_msgpack.c
    ../msgpack.c
)
target_include_directories(test_boundary_msgpack PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_msgpack PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_boundary_msgpack PRIVATE m)
enable_testing()
add_test(NAME msgpack_tests COMMAND test_msgpack)
add_test(NAME msgpack_null_tests COMMAND test_null_msgpack)
add_test(NAME msgpack_boundary_tests COMMAND test_boundary_msgpack)
