cmake_minimum_required(VERSION 3.13)
project(test_sd_spi C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sd_spi
    test_sd_spi.c
    ../sd_spi.c
)

target_include_directories(test_sd_spi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_sd_spi PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME sd_spi_tests COMMAND test_sd_spi)

add_executable(prop_sd_spi
    prop_sd_spi.c
    ../sd_spi.c
)
target_include_directories(prop_sd_spi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_sd_spi PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME sd_spi_prop_tests COMMAND prop_sd_spi)

add_executable(test_hw_model_sd_spi
    test_hw_model_sd_spi.c
    ../sd_spi.c
)
target_include_directories(test_hw_model_sd_spi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_sd_spi PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)

add_test(NAME sd_spi_hw_model_tests COMMAND test_hw_model_sd_spi)
