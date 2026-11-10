cmake_minimum_required(VERSION 3.13)
project(test_timer_wheel C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_timer_wheel
    test_timer_wheel.c
    ../timer_wheel.c
)

target_include_directories(test_timer_wheel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_timer_wheel PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_timer_wheel
    test_null_timer_wheel.c
    ../timer_wheel.c
)
target_include_directories(test_null_timer_wheel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_timer_wheel PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_timer_wheel
    test_boundary_timer_wheel.c
    ../timer_wheel.c
)
target_include_directories(test_boundary_timer_wheel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_timer_wheel PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# A host thread stands in for the tick ISR. -O3 -flto lets the compiler see
# through the calls, which is what a missing volatile needs to bite.
find_package(Threads REQUIRED)
add_executable(test_race_timer_wheel
    test_race_timer_wheel.c
    ../timer_wheel.c
)
target_include_directories(test_race_timer_wheel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_race_timer_wheel PRIVATE -O3 -flto -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_options(test_race_timer_wheel PRIVATE -O3 -flto)
target_link_libraries(test_race_timer_wheel PRIVATE Threads::Threads)

enable_testing()
add_test(NAME timer_wheel_tests COMMAND test_timer_wheel)
add_test(NAME timer_wheel_null_tests COMMAND test_null_timer_wheel)
add_test(NAME timer_wheel_boundary_tests COMMAND test_boundary_timer_wheel)
add_test(NAME timer_wheel_race_tests COMMAND test_race_timer_wheel)
