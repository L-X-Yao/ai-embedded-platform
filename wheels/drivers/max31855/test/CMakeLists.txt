cmake_minimum_required(VERSION 3.13)
project(test_max31855 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_max31855
    test_max31855.c
    ../max31855.c
)

target_include_directories(test_max31855 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_max31855 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_max31855 PRIVATE m)


add_executable(test_null_max31855
    test_null_max31855.c
    ../max31855.c
)
target_include_directories(test_null_max31855 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_max31855 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME max31855_tests COMMAND test_max31855)

add_executable(prop_max31855
    prop_max31855.c
    ../max31855.c
)
target_include_directories(prop_max31855 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_max31855 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_max31855 PRIVATE m)

add_test(NAME max31855_prop_tests COMMAND prop_max31855)

add_executable(test_hw_model_max31855
    test_hw_model_max31855.c
    ../max31855.c
)
target_include_directories(test_hw_model_max31855 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_max31855 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_max31855 PRIVATE m)

add_test(NAME max31855_hw_model_tests COMMAND test_hw_model_max31855)
add_test(NAME max31855_null_tests COMMAND test_null_max31855)
