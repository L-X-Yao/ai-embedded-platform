cmake_minimum_required(VERSION 3.13)
project(test_hcsr04 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hcsr04
    test_hcsr04.c
    ../hcsr04.c
)

target_include_directories(test_hcsr04 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hcsr04 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_hcsr04 PRIVATE m)


add_executable(test_null_hcsr04
    test_null_hcsr04.c
    ../hcsr04.c
)
target_include_directories(test_null_hcsr04 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_hcsr04 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME hcsr04_tests COMMAND test_hcsr04)
add_test(NAME hcsr04_null_tests COMMAND test_null_hcsr04)
