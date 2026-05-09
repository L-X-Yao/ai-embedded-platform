cmake_minimum_required(VERSION 3.13)
project(test_mlx90614 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mlx90614
    test_mlx90614.c
    ../mlx90614.c
)

target_include_directories(test_mlx90614 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mlx90614 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_hw_model_mlx90614
    test_hw_model_mlx90614.c
    ../mlx90614.c
)
target_include_directories(test_hw_model_mlx90614 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_hw_model_mlx90614 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function)
target_link_libraries(test_hw_model_mlx90614 PRIVATE m)


add_executable(test_null_mlx90614
    test_null_mlx90614.c
    ../mlx90614.c
)
target_include_directories(test_null_mlx90614 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_mlx90614 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME mlx90614_tests COMMAND test_mlx90614)
add_test(NAME mlx90614_hw_model_tests COMMAND test_hw_model_mlx90614)

add_executable(prop_mlx90614
    prop_mlx90614.c
    ../mlx90614.c
)
target_include_directories(prop_mlx90614 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(prop_mlx90614 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-unused-function -Wno-unused-parameter)
target_link_libraries(prop_mlx90614 PRIVATE m)

add_test(NAME mlx90614_prop_tests COMMAND prop_mlx90614)
add_test(NAME mlx90614_null_tests COMMAND test_null_mlx90614)
