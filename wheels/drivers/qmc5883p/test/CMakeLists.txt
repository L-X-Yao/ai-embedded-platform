cmake_minimum_required(VERSION 3.13)
project(test_qmc5883p C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_qmc5883p
    test_qmc5883p.c
    ../qmc5883p.c
)

target_include_directories(test_qmc5883p PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_link_libraries(test_qmc5883p PRIVATE m)
target_compile_options(test_qmc5883p PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME qmc5883p_tests COMMAND test_qmc5883p)

add_executable(prop_qmc5883p
    prop_qmc5883p.c
    ../qmc5883p.c
)
target_include_directories(prop_qmc5883p PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_qmc5883p PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(prop_qmc5883p PRIVATE m)

add_test(NAME qmc5883p_prop_tests COMMAND prop_qmc5883p)
