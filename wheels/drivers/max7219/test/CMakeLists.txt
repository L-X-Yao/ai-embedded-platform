cmake_minimum_required(VERSION 3.13)
project(test_max7219 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_max7219
    test_max7219.c
    ../max7219.c
)
target_include_directories(test_max7219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_max7219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_max7219
    test_hw_model_max7219.c
    ../max7219.c
)
target_include_directories(test_hw_model_max7219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_max7219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)


add_executable(test_null_max7219
    test_null_max7219.c
    ../max7219.c
)
target_include_directories(test_null_max7219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_max7219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_max7219
    test_boundary_max7219.c
    ../max7219.c
)
target_include_directories(test_boundary_max7219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_max7219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME max7219_tests COMMAND test_max7219)
add_test(NAME max7219_hw_model_tests COMMAND test_hw_model_max7219)

add_executable(prop_max7219
    prop_max7219.c
    ../max7219.c
)
target_include_directories(prop_max7219 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_max7219 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME max7219_prop_tests COMMAND prop_max7219)
add_test(NAME max7219_null_tests COMMAND test_null_max7219)
add_test(NAME max7219_boundary_tests COMMAND test_boundary_max7219)
