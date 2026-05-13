cmake_minimum_required(VERSION 3.13)
project(smart_lock C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ----- wheel source roots (relative to this CMakeLists.txt) -----
set(WHEELS    ${CMAKE_CURRENT_SOURCE_DIR}/../../wheels)
set(TOOLING   ${CMAKE_CURRENT_SOURCE_DIR}/../../tooling)

set(PRIM      ${WHEELS}/primitives)
set(DRV       ${WHEELS}/drivers)
set(MOD       ${WHEELS}/modules)

# ----- executable: smart_lock -----
add_executable(smart_lock
    smart_lock.c

    # L0 - aes128 (encrypt audit log)
    ${PRIM}/aes128/aes128.c

    # L1 - rc522 (RFID reader, SPI)
    ${DRV}/rc522/rc522.c

    # L1 - cst816s (capacitive touch, I2C)
    ${DRV}/cst816s/cst816s.c

    # L1 - buzzer (PWM tone feedback)
    ${DRV}/buzzer/buzzer.c

    # L3 - event_bus (pub/sub decouples reader/keypad/controller)
    ${MOD}/event_bus/event_bus.c

    # L3 - flash_journal (append-only audit log)
    ${MOD}/flash_journal/flash_journal.c
)

target_include_directories(smart_lock PRIVATE
    # wheel headers
    ${PRIM}/aes128
    ${DRV}/rc522
    ${DRV}/cst816s
    ${DRV}/buzzer
    ${MOD}/event_bus
    ${MOD}/flash_journal

    # test_harness.h + contract.h
    ${TOOLING}/harness
)

target_compile_options(smart_lock PRIVATE
    -Wall -Wextra -Wpedantic -Werror -Wconversion
)

enable_testing()
add_test(NAME smart_lock_integration COMMAND smart_lock)
