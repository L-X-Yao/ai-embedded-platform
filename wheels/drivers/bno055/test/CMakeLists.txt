cmake_minimum_required(VERSION 3.13)
project(test_bno055 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bno055
    test_bno055.c
    ../bno055.c
)

target_include_directories(test_bno055 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bno055 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_bno055 PRIVATE m)


add_executable(test_null_bno055
    test_null_bno055.c
    ../bno055.c
)
target_include_directories(test_null_bno055 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bno055 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bno055_tests COMMAND test_bno055)

# hw_model datasheet test
add_executable(test_hw_model_bno055
    test_hw_model_bno055.c
    ../bno055.c
)
target_include_directories(test_hw_model_bno055 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_bno055 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_bno055 PRIVATE m)
add_test(NAME bno055_hw_model_tests COMMAND test_hw_model_bno055)
add_test(NAME bno055_null_tests COMMAND test_null_bno055)
