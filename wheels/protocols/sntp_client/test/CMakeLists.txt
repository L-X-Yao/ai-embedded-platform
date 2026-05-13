cmake_minimum_required(VERSION 3.13)
project(test_sntp_client C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_sntp_client
    test_sntp_client.c
    ../sntp_client.c
)

target_include_directories(test_sntp_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_sntp_client PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_sntp_client
    test_null_sntp_client.c
    ../sntp_client.c
)
target_include_directories(test_null_sntp_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_sntp_client PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_sntp_client
    test_boundary_sntp_client.c
    ../sntp_client.c
)
target_include_directories(test_boundary_sntp_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_sntp_client PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME sntp_client_tests COMMAND test_sntp_client)
add_test(NAME sntp_client_null_tests COMMAND test_null_sntp_client)
add_test(NAME sntp_client_boundary_tests COMMAND test_boundary_sntp_client)
