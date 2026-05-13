cmake_minimum_required(VERSION 3.13)
project(test_lora_sx1276 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_lora_sx1276
    test_lora_sx1276.c
    ../lora_sx1276.c
)

target_include_directories(test_lora_sx1276 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_lora_sx1276 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_boundary_lora_sx1276
    test_boundary_lora_sx1276.c
    ../lora_sx1276.c
)
target_include_directories(test_boundary_lora_sx1276 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_lora_sx1276 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME lora_sx1276_tests COMMAND test_lora_sx1276)

add_executable(prop_lora_sx1276
    prop_lora_sx1276.c
    ../lora_sx1276.c
)
target_include_directories(prop_lora_sx1276 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_lora_sx1276 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)

add_test(NAME lora_sx1276_prop_tests COMMAND prop_lora_sx1276)

add_executable(test_hw_model_lora_sx1276
    test_hw_model_lora_sx1276.c
    ../lora_sx1276.c
)
target_include_directories(test_hw_model_lora_sx1276 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
# -Wno-conversion: hw_model.h in tooling/harness triggers integer-promotion
# warning in hw_model__write_reg; suppressed here since it is a tooling
# header, not driver code.
target_compile_options(test_hw_model_lora_sx1276 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-conversion -Wno-unused-function)

add_test(NAME lora_sx1276_hw_model_tests COMMAND test_hw_model_lora_sx1276)

add_executable(test_null_lora_sx1276
    test_null_lora_sx1276.c
    ../lora_sx1276.c
)
target_include_directories(test_null_lora_sx1276 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_lora_sx1276 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_test(NAME lora_sx1276_null_tests COMMAND test_null_lora_sx1276)
add_test(NAME lora_sx1276_boundary_tests COMMAND test_boundary_lora_sx1276)
