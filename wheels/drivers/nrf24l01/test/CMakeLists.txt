cmake_minimum_required(VERSION 3.13)
project(test_nrf24l01 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_nrf24l01
    test_nrf24l01.c
    ../nrf24l01.c
)

target_include_directories(test_nrf24l01 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_nrf24l01 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME nrf24l01_tests COMMAND test_nrf24l01)

add_executable(prop_nrf24l01
    prop_nrf24l01.c
    ../nrf24l01.c
)
target_include_directories(prop_nrf24l01 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_nrf24l01 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME nrf24l01_prop_tests COMMAND prop_nrf24l01)

add_executable(test_hw_model_nrf24l01
    test_hw_model_nrf24l01.c
    ../nrf24l01.c
)
target_include_directories(test_hw_model_nrf24l01 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_nrf24l01 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_nrf24l01 PRIVATE m)

add_test(NAME nrf24l01_hw_model_tests COMMAND test_hw_model_nrf24l01)
