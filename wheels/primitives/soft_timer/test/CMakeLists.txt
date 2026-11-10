cmake_minimum_required(VERSION 3.13)
project(test_soft_timer C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_soft_timer
    test_soft_timer.c
    ../soft_timer.c
)

target_include_directories(test_soft_timer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_soft_timer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_soft_timer
    test_null_soft_timer.c
    ../soft_timer.c
)
target_include_directories(test_null_soft_timer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_soft_timer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_soft_timer
    test_boundary_soft_timer.c
    ../soft_timer.c
)
target_include_directories(test_boundary_soft_timer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_soft_timer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# A host thread stands in for the tick ISR. -O2 -flto lets the compiler see
# through the calls, which is what a missing volatile needs to bite.
find_package(Threads REQUIRED)
add_executable(test_race_soft_timer
    test_race_soft_timer.c
    ../soft_timer.c
)
target_include_directories(test_race_soft_timer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_race_soft_timer PRIVATE -O2 -flto -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_options(test_race_soft_timer PRIVATE -O2 -flto)
target_link_libraries(test_race_soft_timer PRIVATE Threads::Threads)

# Builds soft_timer.c itself, to hook memset.
add_executable(test_destroy_tick_soft_timer
    test_destroy_tick_soft_timer.c
)
target_include_directories(test_destroy_tick_soft_timer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_destroy_tick_soft_timer PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

enable_testing()
add_test(NAME soft_timer_tests COMMAND test_soft_timer)
add_test(NAME soft_timer_null_tests COMMAND test_null_soft_timer)
add_test(NAME soft_timer_boundary_tests COMMAND test_boundary_soft_timer)
add_test(NAME soft_timer_race_tests COMMAND test_race_soft_timer)
add_test(NAME soft_timer_destroy_tick_tests COMMAND test_destroy_tick_soft_timer)
