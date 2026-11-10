cmake_minimum_required(VERSION 3.13)
project(test_ring_buffer C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ring_buffer
    test_ring_buffer.c
    ../ring_buffer.c
)

target_include_directories(test_ring_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ring_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_ring_buffer
    test_null_ring_buffer.c
    ../ring_buffer.c
)
target_include_directories(test_null_ring_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ring_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_ring_buffer
    test_boundary_ring_buffer.c
    ../ring_buffer.c
)
target_include_directories(test_boundary_ring_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_ring_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# Host threads stand in for the ISR and the main loop.
find_package(Threads REQUIRED)
add_executable(test_race_ring_buffer
    test_race_ring_buffer.c
    ../ring_buffer.c
)
target_include_directories(test_race_ring_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_race_ring_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_race_ring_buffer PRIVATE Threads::Threads)

enable_testing()
add_test(NAME ring_buffer_tests COMMAND test_ring_buffer)
add_test(NAME ring_buffer_null_tests COMMAND test_null_ring_buffer)
add_test(NAME ring_buffer_boundary_tests COMMAND test_boundary_ring_buffer)
add_test(NAME ring_buffer_race_tests COMMAND test_race_ring_buffer)
