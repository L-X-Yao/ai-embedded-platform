cmake_minimum_required(VERSION 3.13)
project(test_iir_lowpass C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(TOOLING_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness)

# --- main tests ---
add_executable(test_iir_lowpass
    test_iir_lowpass.c
    ../iir_lowpass.c
)

target_include_directories(test_iir_lowpass PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_iir_lowpass PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_iir_lowpass PRIVATE m)

# --- null parameter tests ---
add_executable(test_null_iir_lowpass
    test_null_iir_lowpass.c
    ../iir_lowpass.c
)

target_include_directories(test_null_iir_lowpass PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${TOOLING_TEST_DIR}
)
target_compile_options(test_null_iir_lowpass PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_boundary_iir_lowpass
    test_boundary_iir_lowpass.c
    ../iir_lowpass.c
)
target_include_directories(test_boundary_iir_lowpass PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_iir_lowpass PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_boundary_iir_lowpass PRIVATE m)
enable_testing()
add_test(NAME iir_lowpass_tests      COMMAND test_iir_lowpass)
add_test(NAME iir_lowpass_null_tests COMMAND test_null_iir_lowpass)
add_test(NAME iir_lowpass_boundary_tests COMMAND test_boundary_iir_lowpass)
