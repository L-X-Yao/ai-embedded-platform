cmake_minimum_required(VERSION 3.13)
project(test_hmc5883l C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hmc5883l
    test_hmc5883l.c
    ../hmc5883l.c
)
target_include_directories(test_hmc5883l PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hmc5883l PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_hmc5883l PRIVATE m)


add_executable(test_null_hmc5883l
    test_null_hmc5883l.c
    ../hmc5883l.c
)
target_include_directories(test_null_hmc5883l PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_hmc5883l PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME hmc5883l_tests COMMAND test_hmc5883l)

add_executable(prop_hmc5883l
    prop_hmc5883l.c
    ../hmc5883l.c
)
target_include_directories(prop_hmc5883l PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_hmc5883l PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_hmc5883l PRIVATE m)

add_test(NAME hmc5883l_prop_tests COMMAND prop_hmc5883l)

add_executable(test_hw_model_hmc5883l
    test_hw_model_hmc5883l.c
    ../hmc5883l.c
)
target_include_directories(test_hw_model_hmc5883l PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_hmc5883l PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_hmc5883l PRIVATE m)

add_test(NAME hmc5883l_hw_model_tests COMMAND test_hw_model_hmc5883l)
add_test(NAME hmc5883l_null_tests COMMAND test_null_hmc5883l)
