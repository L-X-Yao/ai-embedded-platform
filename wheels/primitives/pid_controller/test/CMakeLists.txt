cmake_minimum_required(VERSION 3.13)
project(test_pid_controller C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_pid_controller
    test_pid_controller.c
    ../pid_controller.c
)

target_include_directories(test_pid_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_pid_controller PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_pid_controller PRIVATE m)


add_executable(test_null_pid_controller
    test_null_pid_controller.c
    ../pid_controller.c
)
target_include_directories(test_null_pid_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_pid_controller PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_pid_controller
    test_boundary_pid_controller.c
    ../pid_controller.c
)
target_include_directories(test_boundary_pid_controller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_pid_controller PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_boundary_pid_controller PRIVATE m)
enable_testing()
add_test(NAME pid_controller_tests COMMAND test_pid_controller)
add_test(NAME pid_controller_null_tests COMMAND test_null_pid_controller)
add_test(NAME pid_controller_boundary_tests COMMAND test_boundary_pid_controller)
