cmake_minimum_required(VERSION 3.13)
project(test_is31fl3731 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_is31fl3731
    test_is31fl3731.c
    ../is31fl3731.c
)

target_include_directories(test_is31fl3731 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_is31fl3731 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_is31fl3731 PRIVATE CONTRACT_ENABLED)


add_executable(test_null_is31fl3731
    test_null_is31fl3731.c
    ../is31fl3731.c
)
target_include_directories(test_null_is31fl3731 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_null_is31fl3731 PRIVATE CONTRACT_ENABLED)
target_compile_options(test_null_is31fl3731 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_is31fl3731
    test_boundary_is31fl3731.c
    ../is31fl3731.c
)
target_include_directories(test_boundary_is31fl3731 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_definitions(test_boundary_is31fl3731 PRIVATE CONTRACT_ENABLED)
target_compile_options(test_boundary_is31fl3731 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME is31fl3731_tests COMMAND test_is31fl3731)

# hw_model datasheet test
add_executable(test_hw_model_is31fl3731
    test_hw_model_is31fl3731.c
    ../is31fl3731.c
)
target_include_directories(test_hw_model_is31fl3731 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_is31fl3731 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_compile_definitions(test_hw_model_is31fl3731 PRIVATE CONTRACT_ENABLED)
add_test(NAME is31fl3731_hw_model_tests COMMAND test_hw_model_is31fl3731)
add_test(NAME is31fl3731_null_tests COMMAND test_null_is31fl3731)
add_test(NAME is31fl3731_boundary_tests COMMAND test_boundary_is31fl3731)
