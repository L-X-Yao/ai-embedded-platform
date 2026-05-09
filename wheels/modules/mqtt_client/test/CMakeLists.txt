cmake_minimum_required(VERSION 3.13)
project(test_mqtt_client C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_mqtt_client
    test_mqtt_client.c
    ../mqtt_client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/protocols/mqtt_packet/mqtt_packet.c
)

target_include_directories(test_mqtt_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/protocols/mqtt_packet
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_mqtt_client PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_mqtt_client
    test_null_mqtt_client.c
    ../mqtt_client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/protocols/mqtt_packet/mqtt_packet.c
)
target_include_directories(test_null_mqtt_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/protocols/mqtt_packet
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)
target_compile_options(test_null_mqtt_client PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME mqtt_client_tests COMMAND test_mqtt_client)
add_test(NAME mqtt_client_null_tests COMMAND test_null_mqtt_client)
