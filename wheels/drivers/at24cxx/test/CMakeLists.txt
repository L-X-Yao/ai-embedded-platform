cmake_minimum_required(VERSION 3.13)
project(test_at24cxx C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_at24cxx
    test_at24cxx.c
    ../at24cxx.c
)

target_include_directories(test_at24cxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_at24cxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(prop_at24cxx
    prop_at24cxx.c
    ../at24cxx.c
)
target_include_directories(prop_at24cxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_at24cxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)


add_executable(test_null_at24cxx
    test_null_at24cxx.c
    ../at24cxx.c
)
target_include_directories(test_null_at24cxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_at24cxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_at24cxx
    test_boundary_at24cxx.c
    ../at24cxx.c
)
target_include_directories(test_boundary_at24cxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_at24cxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME at24cxx_tests COMMAND test_at24cxx)
add_test(NAME at24cxx_prop_tests COMMAND prop_at24cxx)

add_executable(test_hw_model_at24cxx
    test_hw_model_at24cxx.c
    ../at24cxx.c
)
target_include_directories(test_hw_model_at24cxx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_at24cxx PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_at24cxx PRIVATE m)

add_test(NAME at24cxx_hw_model_tests COMMAND test_hw_model_at24cxx)
add_test(NAME at24cxx_null_tests COMMAND test_null_at24cxx)
add_test(NAME at24cxx_boundary_tests COMMAND test_boundary_at24cxx)
