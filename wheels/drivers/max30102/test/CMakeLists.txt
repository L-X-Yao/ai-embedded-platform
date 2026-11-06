cmake_minimum_required(VERSION 3.13)
project(test_max30102 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_max30102
    test_max30102.c
    ../max30102.c
)

target_include_directories(test_max30102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_max30102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_max30102 PRIVATE m)

add_executable(test_null_max30102
    test_null_max30102.c
    ../max30102.c
)

target_include_directories(test_null_max30102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_max30102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_null_max30102 PRIVATE m)


add_executable(test_boundary_max30102
    test_boundary_max30102.c
    ../max30102.c
)
target_include_directories(test_boundary_max30102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_max30102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME max30102_tests COMMAND test_max30102)
add_test(NAME max30102_null_tests COMMAND test_null_max30102)
add_test(NAME max30102_boundary_tests COMMAND test_boundary_max30102)

add_executable(test_hw_model_max30102
    test_hw_model_max30102.c
    ../max30102.c
)
target_include_directories(test_hw_model_max30102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_max30102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_hw_model_max30102 PRIVATE m)

add_test(NAME max30102_hw_model_tests COMMAND test_hw_model_max30102)
