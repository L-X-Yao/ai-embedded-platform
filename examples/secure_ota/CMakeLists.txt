cmake_minimum_required(VERSION 3.13)
project(secure_ota C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(MOD       ${WHEELS}/modules)

# ----- executable: secure_ota -----
add_executable(secure_ota
    secure_ota.c

    # L0 - aes128
    ${PRIM}/aes128/aes128.c

    # L0 - sha256
    ${PRIM}/sha256/sha256.c

    # L0 - hmac_sha256 (depends on sha256)
    ${PRIM}/hmac_sha256/hmac_sha256.c

    # L0 - crc32
    ${PRIM}/crc32/crc32.c

    # L3 - boot_counter
    ${MOD}/boot_counter/boot_counter.c

    # L3 - ota_manager
    ${MOD}/ota_manager/ota_manager.c

    # L3 - flash_journal
    ${MOD}/flash_journal/flash_journal.c
)

target_include_directories(secure_ota PRIVATE
    # wheel headers
    ${PRIM}/aes128
    ${PRIM}/sha256
    ${PRIM}/hmac_sha256
    ${PRIM}/crc32
    ${MOD}/boot_counter
    ${MOD}/ota_manager
    ${MOD}/flash_journal

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(secure_ota PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME secure_ota_integration COMMAND secure_ota)
