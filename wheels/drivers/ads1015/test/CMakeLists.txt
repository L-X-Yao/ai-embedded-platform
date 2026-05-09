cmake_minimum_required(VERSION 3.13)
project(test_ads1015 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_ads1015
    test_ads1015.c
    ../ads1015.c
)

target_include_directories(test_ads1015 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_ads1015 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_ads1015 PRIVATE m)

add_executable(test_hw_model_ads1015
    test_hw_model_ads1015.c
    ../ads1015.c
)
target_include_directories(test_hw_model_ads1015 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_hw_model_ads1015 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_ads1015 PRIVATE m)


add_executable(test_null_ads1015
    test_null_ads1015.c
    ../ads1015.c
)
target_include_directories(test_null_ads1015 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_ads1015 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME ads1015_tests COMMAND test_ads1015)
add_test(NAME ads1015_hw_model_tests COMMAND test_hw_model_ads1015)

add_executable(prop_ads1015
    prop_ads1015.c
    ../ads1015.c
)
target_include_directories(prop_ads1015 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(prop_ads1015 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_ads1015 PRIVATE m)

add_test(NAME ads1015_prop_tests COMMAND prop_ads1015)
add_test(NAME ads1015_null_tests COMMAND test_null_ads1015)
