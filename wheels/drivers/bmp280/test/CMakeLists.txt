cmake_minimum_required(VERSION 3.13)
project(test_bmp280 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bmp280
    test_bmp280.c
    ../bmp280.c
)

target_include_directories(test_bmp280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bmp280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_bmp280 PRIVATE m)

add_executable(prop_bmp280
    prop_bmp280.c
    ../bmp280.c
)
target_include_directories(prop_bmp280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_bmp280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(prop_bmp280 PRIVATE m)


add_executable(test_null_bmp280
    test_null_bmp280.c
    ../bmp280.c
)
target_include_directories(test_null_bmp280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bmp280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bmp280_tests COMMAND test_bmp280)
add_test(NAME bmp280_prop_tests COMMAND prop_bmp280)

add_executable(test_hw_model_bmp280
    test_hw_model_bmp280.c
    ../bmp280.c
)
target_include_directories(test_hw_model_bmp280 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_bmp280 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_bmp280 PRIVATE m)

add_test(NAME bmp280_hw_model_tests COMMAND test_hw_model_bmp280)
add_test(NAME bmp280_null_tests COMMAND test_null_bmp280)
