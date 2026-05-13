cmake_minimum_required(VERSION 3.13)
project(test_hmac_sha256 C)

# CMAKE_C_STANDARD set as a variable (not target property) intentionally:
# standalone wheel test projects own the entire build, so a top-level variable
# is simpler and consistent across all 140+ wheels.
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_hmac_sha256
    test_hmac_sha256.c
    ../hmac_sha256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/primitives/sha256/sha256.c
)

target_include_directories(test_hmac_sha256 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    # Convention: all wheel tests live at wheels/<layer>/<name>/test/,
    # so ../../../../tooling/harness always resolves to the repo-root tooling dir.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/primitives/sha256
)
target_compile_options(test_hmac_sha256 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)


add_executable(test_null_hmac_sha256
    test_null_hmac_sha256.c
    ../hmac_sha256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/primitives/sha256/sha256.c
)
target_include_directories(test_null_hmac_sha256 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/primitives/sha256
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_null_hmac_sha256 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)

add_executable(test_boundary_hmac_sha256
    test_boundary_hmac_sha256.c
    ../hmac_sha256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/primitives/sha256/sha256.c
)
target_include_directories(test_boundary_hmac_sha256 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../wheels/primitives/sha256
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_boundary_hmac_sha256 PRIVATE -Wall -Wextra -Wpedantic -Werror -Wconversion)
enable_testing()
add_test(NAME hmac_sha256_tests COMMAND test_hmac_sha256)
add_test(NAME hmac_sha256_null_tests COMMAND test_null_hmac_sha256)
add_test(NAME hmac_sha256_boundary_tests COMMAND test_boundary_hmac_sha256)
