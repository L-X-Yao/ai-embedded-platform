cmake_minimum_required(VERSION 3.13)
project(test_qmi8658a C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_qmi8658a
    test_qmi8658a.c
    ../qmi8658a.c
)

target_include_directories(test_qmi8658a PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

target_link_libraries(test_qmi8658a PRIVATE m)
target_compile_options(test_qmi8658a PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME qmi8658a_tests COMMAND test_qmi8658a)

add_executable(prop_qmi8658a
    prop_qmi8658a.c
    ../qmi8658a.c
)
target_include_directories(prop_qmi8658a PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_qmi8658a PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_qmi8658a PRIVATE m)

add_test(NAME qmi8658a_prop_tests COMMAND prop_qmi8658a)
