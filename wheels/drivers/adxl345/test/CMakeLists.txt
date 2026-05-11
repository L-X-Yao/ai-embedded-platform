cmake_minimum_required(VERSION 3.13)
project(test_adxl345 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_adxl345
    test_adxl345.c
    ../adxl345.c
)

target_include_directories(test_adxl345 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_adxl345 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_adxl345 PRIVATE m)


add_executable(test_null_adxl345
    test_null_adxl345.c
    ../adxl345.c
)
target_include_directories(test_null_adxl345 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_adxl345 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME adxl345_tests COMMAND test_adxl345)

# hw_model datasheet test
add_executable(test_hw_model_adxl345
    test_hw_model_adxl345.c
    ../adxl345.c
)
target_include_directories(test_hw_model_adxl345 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_adxl345 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_adxl345 PRIVATE m)
add_test(NAME adxl345_hw_model_tests COMMAND test_hw_model_adxl345)
add_test(NAME adxl345_null_tests COMMAND test_null_adxl345)
