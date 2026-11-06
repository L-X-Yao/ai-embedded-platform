cmake_minimum_required(VERSION 3.13)
project(test_event_queue C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_event_queue
    test_event_queue.c
    ../event_queue.c
)

target_include_directories(test_event_queue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_event_queue PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_event_queue
    test_null_event_queue.c
    ../event_queue.c
)
target_include_directories(test_null_event_queue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_event_queue PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_event_queue
    test_boundary_event_queue.c
    ../event_queue.c
)
target_include_directories(test_boundary_event_queue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_event_queue PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

# Host threads stand in for the ISR and the main loop.
find_package(Threads REQUIRED)
add_executable(test_race_event_queue
    test_race_event_queue.c
    ../event_queue.c
)
target_include_directories(test_race_event_queue PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_race_event_queue PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
target_link_libraries(test_race_event_queue PRIVATE Threads::Threads)

enable_testing()
add_test(NAME event_queue_tests COMMAND test_event_queue)
add_test(NAME event_queue_null_tests COMMAND test_null_event_queue)
add_test(NAME event_queue_boundary_tests COMMAND test_boundary_event_queue)
add_test(NAME event_queue_race_tests COMMAND test_race_event_queue)
