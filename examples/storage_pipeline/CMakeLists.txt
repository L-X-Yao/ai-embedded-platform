cmake_minimum_required(VERSION 3.13)
project(storage_pipeline C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(DRV       ${WHEELS}/drivers)

# ----- FlashDB vendor library (relaxed flags -- upstream code) -----
add_library(flashdb_vendor STATIC
    ${PRIM}/kvs/vendor/src/fdb.c
    ${PRIM}/kvs/vendor/src/fdb_kvdb.c
    ${PRIM}/kvs/vendor/src/fdb_tsdb.c
    ${PRIM}/kvs/vendor/src/fdb_utils.c
)

target_include_directories(flashdb_vendor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PRIM}/kvs/vendor/inc
    ${PRIM}/fal
)

target_compile_options(flashdb_vendor PRIVATE
    -Wall -Wextra -Wpedantic
)

# ----- executable: storage_pipeline -----
add_executable(storage_pipeline
    storage_pipeline.c

    # L0 - fal
    ${PRIM}/fal/fal.c

    # L0 - fifo_queue
    ${PRIM}/fifo_queue/fifo_queue.c

    # L0 - byte_buffer
    ${PRIM}/byte_buffer/byte_buffer.c

    # L0 - string_utils
    ${PRIM}/string_utils/string_utils.c

    # L0 - hash_fnv
    ${PRIM}/hash_fnv/hash_fnv.c

    # L1 - at24cxx
    ${DRV}/at24cxx/at24cxx.c
)

target_include_directories(storage_pipeline PRIVATE
    # local config headers (fal_cfg.h, fdb_cfg.h)
    ${CMAKE_CURRENT_SOURCE_DIR}

    # wheel headers
    ${PRIM}/fal
    ${PRIM}/kvs/vendor/inc
    ${PRIM}/fifo_queue
    ${PRIM}/byte_buffer
    ${PRIM}/string_utils
    ${PRIM}/hash_fnv
    ${DRV}/at24cxx

    # test_harness.h
    ${TOOLING}/harness
)

target_link_libraries(storage_pipeline PRIVATE flashdb_vendor)

target_compile_options(storage_pipeline PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME storage_pipeline_integration COMMAND storage_pipeline)
