cmake_minimum_required(VERSION 3.13)
project(test_moving_average C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_moving_average
    test_moving_average.c
    ../moving_average.c
)

target_include_directories(test_moving_average PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_moving_average PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_moving_average PRIVATE m)


add_executable(test_null_moving_average
    test_null_moving_average.c
    ../moving_average.c
)
target_include_directories(test_null_moving_average PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_moving_average PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME moving_average_tests COMMAND test_moving_average)
add_test(NAME moving_average_null_tests COMMAND test_null_moving_average)
