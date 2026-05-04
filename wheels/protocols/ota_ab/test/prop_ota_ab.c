/*
 * prop_ota_ab.c — property-based tests for ota_ab
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/test prop_ota_ab.c ../ota_ab.c -o prop_ota_ab
 */

#include "test_harness.h"
#include "property_test.h"
#include "../ota_ab.h"
#include <string.h>

/* --- RAM-backed flash mock --- */
#define MOCK_FLASH_SIZE 4096
static uint8_t mock_flash[MOCK_FLASH_SIZE];

static bool mock_erase(void *ctx, uint32_t addr, uint32_t size) {
    (void)ctx;
    if (addr + size > MOCK_FLASH_SIZE) return false;
    memset(mock_flash + addr, 0xFF, size);
    return true;
}

static bool mock_write(void *ctx, uint32_t addr, const uint8_t *data, uint32_t len) {
    (void)ctx;
    if (addr + len > MOCK_FLASH_SIZE) return false;
    memcpy(mock_flash + addr, data, len);
    return true;
}

static bool mock_read(void *ctx, uint32_t addr, uint8_t *buf, uint32_t len) {
    (void)ctx;
    if (addr + len > MOCK_FLASH_SIZE) return false;
    memcpy(buf, mock_flash + addr, len);
    return true;
}

static const ota_ab_flash_t mock_flash_ops = {
    .erase = mock_erase,
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL,
};

static const ota_ab_layout_t layout = {
    .slot_a_addr   = 0,
    .slot_b_addr   = 1024,
    .slot_max_size = 1024,
    .params_addr   = 2048,
    .params_size   = 256,
    .max_boot_count = 3,
};

/* Property: active_slot is always 0 or 1 */
PROPERTY(active_slot_valid, 10000) {
    memset(mock_flash, 0xFF, MOCK_FLASH_SIZE);

    ota_ab_t m;
    memset(&m, 0, sizeof(m));
    ota_ab_init(&m, &mock_flash_ops, &layout);

    /* Random operations */
    uint8_t ops = (uint8_t)ARBI_RANGE(seed, 0, 10);
    for (uint8_t i = 0; i < ops; i++) {
        uint8_t op = ARBI_UINT8(seed) % 4;
        if (op == 0) {
            ota_ab_mark_pending(&m, 512, ARBI_UINT32(seed));
        } else if (op == 1) {
            ota_ab_boot_event(&m);
        } else if (op == 2) {
            ota_ab_confirm(&m);
        } else {
            ota_ab_init(&m, &mock_flash_ops, &layout);
        }
    }

    uint8_t slot = ota_ab_active_slot(&m);
    PROP_ASSERT(slot <= 1);
    uint8_t target = ota_ab_target_slot(&m);
    PROP_ASSERT(target <= 1);
    PROP_ASSERT(target != slot);
}

/* Property: slot_addr returns correct addresses */
PROPERTY(slot_addr_correct, 5000) {
    memset(mock_flash, 0xFF, MOCK_FLASH_SIZE);

    ota_ab_t m;
    memset(&m, 0, sizeof(m));
    ota_ab_init(&m, &mock_flash_ops, &layout);

    PROP_ASSERT_EQ(ota_ab_slot_addr(&m, 0), layout.slot_a_addr);
    PROP_ASSERT_EQ(ota_ab_slot_addr(&m, 1), layout.slot_b_addr);
}

/* Property: crc32 is deterministic */
PROPERTY(crc32_determinism, 10000) {
    uint8_t data[128];
    uint32_t len = (uint32_t)ARBI_RANGE(seed, 0, 128);
    ARBI_FILL(seed, data, len);

    uint32_t c1 = ota_ab_crc32(data, len);
    uint32_t c2 = ota_ab_crc32(data, len);
    PROP_ASSERT_EQ(c1, c2);
}

TEST_MAIN("ota_ab properties") {
    RUN(active_slot_valid);
    RUN(slot_addr_correct);
    RUN(crc32_determinism);
}
TEST_MAIN_END()
