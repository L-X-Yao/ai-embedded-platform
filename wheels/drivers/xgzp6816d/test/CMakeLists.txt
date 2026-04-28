cmake_minimum_required(VERSION 3.13)
project(test_xgzp6816d C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_xgzp6816d
    test_xgzp6816d.c
    ../xgzp6816d.c
)

target_include_directories(test_xgzp6816d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(test_xgzp6816d PRIVATE m)

enable_testing()
add_test(NAME xgzp6816d_tests COMMAND test_xgzp6816d)
