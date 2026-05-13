cmake_minimum_required(VERSION 3.13)
project(test_sht30 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sht30
    test_sht30.c
    ../sht30.c
)

target_include_directories(test_sht30 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_sht30 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_sht30 PRIVATE m)

add_executable(prop_sht30
    prop_sht30.c
    ../sht30.c
)
target_include_directories(prop_sht30 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_sht30 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(prop_sht30 PRIVATE m)

add_executable(test_hw_model_sht30
    test_hw_model_sht30.c
    ../sht30.c
)
target_include_directories(test_hw_model_sht30 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_sht30 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_sht30 PRIVATE m)


add_executable(test_null_sht30
    test_null_sht30.c
    ../sht30.c
)
target_include_directories(test_null_sht30 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_sht30 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_sht30
    test_boundary_sht30.c
    ../sht30.c
)
target_include_directories(test_boundary_sht30 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_sht30 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME sht30_tests COMMAND test_sht30)
add_test(NAME sht30_hw_model_tests COMMAND test_hw_model_sht30)
add_test(NAME sht30_prop_tests COMMAND prop_sht30)
add_test(NAME sht30_null_tests COMMAND test_null_sht30)
add_test(NAME sht30_boundary_tests COMMAND test_boundary_sht30)
