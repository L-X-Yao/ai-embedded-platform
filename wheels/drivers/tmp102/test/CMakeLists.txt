cmake_minimum_required(VERSION 3.13)
project(test_tmp102 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_tmp102
    test_tmp102.c
    ../tmp102.c
)

target_include_directories(test_tmp102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_tmp102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME tmp102_tests COMMAND test_tmp102)

add_executable(prop_tmp102
    prop_tmp102.c
    ../tmp102.c
)
target_include_directories(prop_tmp102 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_tmp102 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_tmp102 PRIVATE m)

add_test(NAME tmp102_prop_tests COMMAND prop_tmp102)
