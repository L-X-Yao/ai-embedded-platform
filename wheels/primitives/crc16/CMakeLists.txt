cmake_minimum_required(VERSION 3.13)
project(crc16_test C)

# Pure software wheel — no VSF dependency. Builds on any C99 host compiler.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(crc16 STATIC crc16.c)
target_include_directories(crc16 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(crc16 PRIVATE -Wall -Wextra -Wpedantic)

add_executable(test_crc16 test_main.c)
target_link_libraries(test_crc16 PRIVATE crc16)
target_compile_options(test_crc16 PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME crc16_tests COMMAND test_crc16)
