cmake_minimum_required(VERSION 3.13)
project(test_mlx90614 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mlx90614
    test_mlx90614.c
    ../mlx90614.c
)

target_include_directories(test_mlx90614 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mlx90614 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME mlx90614_tests COMMAND test_mlx90614)
