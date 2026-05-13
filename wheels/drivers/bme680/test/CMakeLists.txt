cmake_minimum_required(VERSION 3.13)
project(test_bme680 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bme680
    test_bme680.c
    ../bme680.c
)

target_include_directories(test_bme680 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bme680 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_bme680
    test_null_bme680.c
    ../bme680.c
)
target_include_directories(test_null_bme680 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bme680 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_bme680
    test_boundary_bme680.c
    ../bme680.c
)
target_include_directories(test_boundary_bme680 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_bme680 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bme680_tests COMMAND test_bme680)

add_executable(prop_bme680
    prop_bme680.c
    ../bme680.c
)
target_include_directories(prop_bme680 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_bme680 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_bme680 PRIVATE m)

add_test(NAME bme680_prop_tests COMMAND prop_bme680)

add_executable(test_hw_model_bme680
    test_hw_model_bme680.c
    ../bme680.c
)
target_include_directories(test_hw_model_bme680 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_bme680 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_bme680 PRIVATE m)

add_test(NAME bme680_hw_model_tests COMMAND test_hw_model_bme680)
add_test(NAME bme680_null_tests COMMAND test_null_bme680)
add_test(NAME bme680_boundary_tests COMMAND test_boundary_bme680)
