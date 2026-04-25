cmake_minimum_required(VERSION 3.13)
project(test_qmi8658a C)

add_executable(test_qmi8658a
    test_qmi8658a.c
    ../qmi8658a.c
)

target_include_directories(test_qmi8658a PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(test_qmi8658a PRIVATE m)

enable_testing()
add_test(NAME qmi8658a_tests COMMAND test_qmi8658a)
