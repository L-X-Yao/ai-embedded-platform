cmake_minimum_required(VERSION 3.13)
project(test_vl53l0x C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_vl53l0x
    test_vl53l0x.c
    ../vl53l0x.c
)

target_include_directories(test_vl53l0x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_vl53l0x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_vl53l0x
    test_null_vl53l0x.c
    ../vl53l0x.c
)
target_include_directories(test_null_vl53l0x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_vl53l0x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_vl53l0x
    test_boundary_vl53l0x.c
    ../vl53l0x.c
)
target_include_directories(test_boundary_vl53l0x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_vl53l0x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME vl53l0x_tests COMMAND test_vl53l0x)

add_executable(prop_vl53l0x
    prop_vl53l0x.c
    ../vl53l0x.c
)
target_include_directories(prop_vl53l0x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_vl53l0x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME vl53l0x_prop_tests COMMAND prop_vl53l0x)

add_executable(test_hw_model_vl53l0x
    test_hw_model_vl53l0x.c
    ../vl53l0x.c
)
target_include_directories(test_hw_model_vl53l0x PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_vl53l0x PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_vl53l0x PRIVATE m)

add_test(NAME vl53l0x_hw_model_tests COMMAND test_hw_model_vl53l0x)
add_test(NAME vl53l0x_null_tests COMMAND test_null_vl53l0x)
add_test(NAME vl53l0x_boundary_tests COMMAND test_boundary_vl53l0x)
