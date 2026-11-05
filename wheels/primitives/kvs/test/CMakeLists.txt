cmake_minimum_required(VERSION 3.13)
project(test_kvs C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(test_kvs
    test_kvs.c
    # FlashDB vendor
    ../vendor/src/fdb.c
    ../vendor/src/fdb_kvdb.c
    ../vendor/src/fdb_tsdb.c
    ../vendor/src/fdb_utils.c
    # FAL dependency (concrete, ADR-009)
    ../../../primitives/fal/fal.c
)

target_include_directories(test_kvs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../primitives/fal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/harness
)
target_compile_options(test_kvs PRIVATE -Wall -Wextra -Wpedantic)

# Vendored FlashDB is not held to our warning flags
set_source_files_properties(
    ../vendor/src/fdb.c
    ../vendor/src/fdb_kvdb.c
    ../vendor/src/fdb_tsdb.c
    ../vendor/src/fdb_utils.c
    PROPERTIES COMPILE_OPTIONS "-w")

enable_testing()
add_test(NAME kvs_tests COMMAND test_kvs)
