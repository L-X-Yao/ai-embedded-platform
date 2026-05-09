cmake_minimum_required(VERSION 3.13)
project(test_modbus_tcp C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_modbus_tcp
    test_modbus_tcp.c
    ../modbus_tcp.c
)

target_include_directories(test_modbus_tcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_modbus_tcp PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_modbus_tcp
    test_null_modbus_tcp.c
    ../modbus_tcp.c
)
target_include_directories(test_null_modbus_tcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_modbus_tcp PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME modbus_tcp_tests COMMAND test_modbus_tcp)
add_test(NAME modbus_tcp_null_tests COMMAND test_null_modbus_tcp)
