cmake_minimum_required(VERSION 3.13)
project(test_bme280 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bme280
    test_bme280.c
    ../bme280.c
)

target_include_directories(test_bme280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bme280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_bme280 PRIVATE m)

add_executable(prop_bme280
    prop_bme280.c
    ../bme280.c
)
target_include_directories(prop_bme280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_bme280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(prop_bme280 PRIVATE m)


add_executable(test_null_bme280
    test_null_bme280.c
    ../bme280.c
)
target_include_directories(test_null_bme280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bme280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_bme280
    test_boundary_bme280.c
    ../bme280.c
)
target_include_directories(test_boundary_bme280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_bme280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bme280_tests COMMAND test_bme280)
add_test(NAME bme280_prop_tests COMMAND prop_bme280)

add_executable(test_hw_model_bme280
    test_hw_model_bme280.c
    ../bme280.c
)
target_include_directories(test_hw_model_bme280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_bme280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_bme280 PRIVATE m)

add_test(NAME bme280_hw_model_tests COMMAND test_hw_model_bme280)
add_test(NAME bme280_null_tests COMMAND test_null_bme280)
add_test(NAME bme280_boundary_tests COMMAND test_boundary_bme280)
