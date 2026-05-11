cmake_minimum_required(VERSION 3.13)
project(test_sgp40 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sgp40
    test_sgp40.c
    ../sgp40.c
)

target_include_directories(test_sgp40 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_sgp40 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_sgp40 PRIVATE m)


add_executable(test_null_sgp40
    test_null_sgp40.c
    ../sgp40.c
)
target_include_directories(test_null_sgp40 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_sgp40 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME sgp40_tests COMMAND test_sgp40)
add_test(NAME sgp40_null_tests COMMAND test_null_sgp40)
