// SPDX-License-Identifier: MIT
// LibFuzzer harness for ota_ab -- coverage-guided fuzzing
//
// Runs the update state machine over a simulated flash. Parameters read from the
// input are stored with a valid CRC, so damaged fields reach the logic instead of
// being dropped at the CRC check. Every flash access has to stay inside a region.
//
// Compile: clang -fsanitize=fuzzer,address,undefined -I.. -I../../../../tooling/harness \
//          fuzz_libfuzzer_ota_ab.c ../ota_ab.c -o fuzz_ota_ab
// Run:     ./fuzz_ota_ab -max_total_time=60 corpus_ota_ab/

#include "../ota_ab.h"
#include "fuzz_input.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define SLOT_SIZE   256u
#define PARAMS_SIZE 64u
#define SLOT_A      0u
#define SLOT_B      SLOT_SIZE
#define PARAMS_ADDR (2u * SLOT_SIZE)
#define PARAMS_ADDR2 (PARAMS_ADDR + PARAMS_SIZE)
#define FLASH_SIZE  (PARAMS_ADDR2 + PARAMS_SIZE)

static uint8_t g_flash[FLASH_SIZE];
static bool g_flash_fail;

// A region is one slot or a parameter page; no access may cross out of it.
static void check_access(uint32_t addr, uint32_t len) {
    uint64_t end = (uint64_t)addr + len;
    uint32_t start = addr >= PARAMS_ADDR2 ? PARAMS_ADDR2
                   : addr >= PARAMS_ADDR ? PARAMS_ADDR
                   : (addr >= SLOT_B ? SLOT_B : SLOT_A);
    uint32_t size = addr >= PARAMS_ADDR ? PARAMS_SIZE : SLOT_SIZE;
    if (addr >= FLASH_SIZE || end > (uint64_t)start + size) __builtin_trap();
}

static bool flash_erase(void *ctx, uint32_t addr, uint32_t size) {
    (void)ctx;
    check_access(addr, size);
    if (g_flash_fail) return false;
    memset(&g_flash[addr], 0xFF, size);
    return true;
}

static bool flash_write(void *ctx, uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)ctx;
    check_access(addr, len);
    if (g_flash_fail) return false;
    memcpy(&g_flash[addr], data, len);
    return true;
}

static bool flash_read(void *ctx, uint32_t addr, uint8_t *buf, uint32_t len) {
    (void)ctx;
    check_access(addr, len);
    if (g_flash_fail) return false;
    memcpy(buf, &g_flash[addr], len);
    return true;
}

static const ota_ab_flash_t flash = { flash_erase, flash_write, flash_read, NULL };

// params_addr2 comes from the input: one copy or two.
static ota_ab_layout_t layout = {
    .slot_a_addr = SLOT_A, .slot_b_addr = SLOT_B, .slot_max_size = SLOT_SIZE,
    .params_addr = PARAMS_ADDR, .params_size = PARAMS_SIZE, .max_boot_count = 3,
};

static void store_params(fuzz_input_t *in) {
    ota_ab_params_t params;
    uint32_t addr = (fuzz_u8(in) & 1) ? PARAMS_ADDR2 : PARAMS_ADDR;
    memset(&params, 0, sizeof(params));
    fuzz_take(in, (uint8_t *)&params, sizeof(params) - sizeof(params.crc32));
    params.magic = OTA_AB_PARAMS_MAGIC;
    params.crc32 = ota_ab_crc32((const uint8_t *)&params, sizeof(params) - sizeof(params.crc32));
    memcpy(&g_flash[addr], &params, sizeof(params));
}

static void mark_valid_image(ota_ab_t *m, fuzz_input_t *in) {
    uint32_t size = 1u + fuzz_u8(in) % SLOT_SIZE;
    uint32_t base = ota_ab_slot_addr(m, ota_ab_target_slot(m));
    ota_ab_mark_pending(m, size, ota_ab_crc32(&g_flash[base], size));
}

static void check_state(const ota_ab_t *m) {
    if (ota_ab_active_slot(m) > 1) __builtin_trap();
    if (ota_ab_target_slot(m) != 1 - ota_ab_active_slot(m)) __builtin_trap();
}

// Each record is an op byte, bits 0-2 pick the action, bit 7 fails the flash.
static void run_ops(ota_ab_t *m, fuzz_input_t *in) {
    while (in->left > 0) {
        uint8_t op = fuzz_u8(in);
        g_flash_fail = (op & 0x80) != 0;

        switch (op & 7) {
        case 0: mark_valid_image(m, in); break;
        case 1: ota_ab_mark_pending(m, ((uint32_t)fuzz_u16(in) << 4) | (op >> 4), fuzz_u16(in)); break;
        case 2: ota_ab_boot_event(m); break;
        case 3: ota_ab_confirm(m); break;
        case 4: g_flash[fuzz_u16(in) % FLASH_SIZE] ^= (uint8_t)(1u << (op >> 5)); break;
        case 5: store_params(in); break;
        default: ota_ab_init(m, &flash, &layout); break;
        }
        check_state(m);
    }
    g_flash_fail = false;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_input_t in;
    fuzz_input_init(&in, data, size);

    memset(g_flash, 0xFF, sizeof(g_flash));
    g_flash_fail = false;
    layout.params_addr2 = (fuzz_u8(&in) & 1) ? PARAMS_ADDR2 : 0;
    fuzz_take(&in, g_flash, SLOT_SIZE / 4);
    store_params(&in);

    ota_ab_t m;
    memset(&m, 0, sizeof(m));
    if (ota_ab_init(&m, &flash, &layout) != OTA_AB_OK) __builtin_trap();
    check_state(&m);
    run_ops(&m, &in);

    return 0;
}
