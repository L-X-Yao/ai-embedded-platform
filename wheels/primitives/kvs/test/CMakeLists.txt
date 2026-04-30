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
    ../../../primitives/fal/vendor/src/fal.c
    ../../../primitives/fal/vendor/src/fal_flash.c
    ../../../primitives/fal/vendor/src/fal_partition.c
)

target_include_directories(test_kvs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../vendor/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../primitives/fal/vendor/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../tooling/test
)

enable_testing()
add_test(NAME kvs_tests COMMAND test_kvs)
