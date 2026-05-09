cmake_minimum_required(VERSION 3.13)
project(test_enc28j60 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_enc28j60
    test_enc28j60.c
    ../enc28j60.c
)

target_include_directories(test_enc28j60 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_enc28j60 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_enc28j60
    test_null_enc28j60.c
    ../enc28j60.c
)
target_include_directories(test_null_enc28j60 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_enc28j60 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME enc28j60_tests COMMAND test_enc28j60)

add_executable(prop_enc28j60
    prop_enc28j60.c
    ../enc28j60.c
)
target_include_directories(prop_enc28j60 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_enc28j60 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME enc28j60_prop_tests COMMAND prop_enc28j60)

add_executable(test_hw_model_enc28j60
    test_hw_model_enc28j60.c
    ../enc28j60.c
)
target_include_directories(test_hw_model_enc28j60 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_enc28j60 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)

add_test(NAME enc28j60_hw_model_tests COMMAND test_hw_model_enc28j60)
add_test(NAME enc28j60_null_tests COMMAND test_null_enc28j60)
