cmake_minimum_required(VERSION 3.13)
project(test_fal C)

add_executable(test_fal
    test_fal.c
    ../vendor/src/fal.c
    ../vendor/src/fal_flash.c
    ../vendor/src/fal_partition.c
)

target_include_directories(test_fal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/inc
)

enable_testing()
add_test(NAME fal_tests COMMAND test_fal)
