cmake_minimum_required(VERSION 3.13)
project(test_lis3mdl C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lis3mdl
    test_lis3mdl.c
    ../lis3mdl.c
)
target_include_directories(test_lis3mdl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_lis3mdl PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_lis3mdl PRIVATE m)

add_executable(test_null_lis3mdl
    test_null_lis3mdl.c
    ../lis3mdl.c
)
target_include_directories(test_null_lis3mdl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_lis3mdl PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_lis3mdl
    prop_lis3mdl.c
    ../lis3mdl.c
)
target_include_directories(prop_lis3mdl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_lis3mdl PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_lis3mdl PRIVATE m)

add_executable(test_hw_model_lis3mdl
    test_hw_model_lis3mdl.c
    ../lis3mdl.c
)
target_include_directories(test_hw_model_lis3mdl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_lis3mdl PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_lis3mdl PRIVATE m)


add_executable(test_boundary_lis3mdl
    test_boundary_lis3mdl.c
    ../lis3mdl.c
)
target_include_directories(test_boundary_lis3mdl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_lis3mdl PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME lis3mdl_tests COMMAND test_lis3mdl)
add_test(NAME lis3mdl_null_tests COMMAND test_null_lis3mdl)
add_test(NAME lis3mdl_prop_tests COMMAND prop_lis3mdl)
add_test(NAME lis3mdl_hw_model_tests COMMAND test_hw_model_lis3mdl)
add_test(NAME lis3mdl_boundary_tests COMMAND test_boundary_lis3mdl)
