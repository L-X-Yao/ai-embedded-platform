cmake_minimum_required(VERSION 3.13)
project(test_double_buffer C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_double_buffer
    test_double_buffer.c
    ../double_buffer.c
)

target_include_directories(test_double_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_double_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_double_buffer
    test_null_double_buffer.c
    ../double_buffer.c
)
target_include_directories(test_null_double_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_double_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_double_buffer
    test_boundary_double_buffer.c
    ../double_buffer.c
)
target_include_directories(test_boundary_double_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_double_buffer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# Host threads stand in for the ISR and the main loop. -O2 -flto lets the
# compiler see through the calls, which is what a missing volatile needs to bite.
find_package(Threads REQUIRED)
add_executable(test_race_double_buffer
    test_race_double_buffer.c
    ../double_buffer.c
)
target_include_directories(test_race_double_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_race_double_buffer PRIVATE -O2 -flto -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_options(test_race_double_buffer PRIVATE -O2 -flto)
target_link_libraries(test_race_double_buffer PRIVATE Threads::Threads)

enable_testing()
add_test(NAME double_buffer_tests COMMAND test_double_buffer)
add_test(NAME double_buffer_null_tests COMMAND test_null_double_buffer)
add_test(NAME double_buffer_boundary_tests COMMAND test_boundary_double_buffer)
add_test(NAME double_buffer_race_tests COMMAND test_race_double_buffer)
