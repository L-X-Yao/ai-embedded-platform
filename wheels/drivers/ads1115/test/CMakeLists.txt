cmake_minimum_required(VERSION 3.13)
project(test_ads1115 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ads1115
    test_ads1115.c
    ../ads1115.c
)
target_include_directories(test_ads1115 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ads1115 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ads1115 PRIVATE m)

add_executable(prop_ads1115
    prop_ads1115.c
    ../ads1115.c
)
target_include_directories(prop_ads1115 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_ads1115 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(prop_ads1115 PRIVATE m)


add_executable(test_null_ads1115
    test_null_ads1115.c
    ../ads1115.c
)
target_include_directories(test_null_ads1115 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ads1115 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_ads1115
    test_boundary_ads1115.c
    ../ads1115.c
)
target_include_directories(test_boundary_ads1115 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_ads1115 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ads1115_tests COMMAND test_ads1115)
add_test(NAME ads1115_prop_tests COMMAND prop_ads1115)

add_executable(test_hw_model_ads1115
    test_hw_model_ads1115.c
    ../ads1115.c
)
target_include_directories(test_hw_model_ads1115 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_ads1115 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_ads1115 PRIVATE m)

add_test(NAME ads1115_hw_model_tests COMMAND test_hw_model_ads1115)
add_test(NAME ads1115_null_tests COMMAND test_null_ads1115)
add_test(NAME ads1115_boundary_tests COMMAND test_boundary_ads1115)
