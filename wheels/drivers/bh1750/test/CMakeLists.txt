cmake_minimum_required(VERSION 3.13)
project(test_bh1750 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_bh1750
    test_bh1750.c
    ../bh1750.c
)
target_include_directories(test_bh1750 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_bh1750 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_bh1750 PRIVATE m)


add_executable(test_null_bh1750
    test_null_bh1750.c
    ../bh1750.c
)
target_include_directories(test_null_bh1750 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_bh1750 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_bh1750
    test_boundary_bh1750.c
    ../bh1750.c
)
target_include_directories(test_boundary_bh1750 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_bh1750 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME bh1750_tests COMMAND test_bh1750)

add_executable(prop_bh1750
    prop_bh1750.c
    ../bh1750.c
)
target_include_directories(prop_bh1750 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_bh1750 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_bh1750 PRIVATE m)

add_test(NAME bh1750_prop_tests COMMAND prop_bh1750)

add_executable(test_hw_model_bh1750
    test_hw_model_bh1750.c
    ../bh1750.c
)
target_include_directories(test_hw_model_bh1750 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_bh1750 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_bh1750 PRIVATE m)

add_test(NAME bh1750_hw_model_tests COMMAND test_hw_model_bh1750)
add_test(NAME bh1750_null_tests COMMAND test_null_bh1750)
add_test(NAME bh1750_boundary_tests COMMAND test_boundary_bh1750)
