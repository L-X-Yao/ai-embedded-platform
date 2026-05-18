// SPDX-License-Identifier: MIT
/*
 * prop_i2c_scan.c — property-based tests for i2c_scan
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_i2c_scan.c ../i2c_scan.c -o prop_i2c_scan
 */

#include "test_harness.h"
#include "property_test.h"
#include "../i2c_scan.h"
#include <string.h>

/* --- mock probe: ACKs addresses based on seed bitmask --- */
static uint32_t g_ack_mask_lo;
static uint32_t g_ack_mask_mi;
static uint32_t g_ack_mask_hi;

static bool mock_probe(void *ctx, uint8_t addr) {
    (void)ctx;
    if (addr < 32)  return (g_ack_mask_lo >> addr) & 1;
    if (addr < 64)  return (g_ack_mask_mi >> (addr - 32)) & 1;
    if (addr < 96)  return (g_ack_mask_hi >> (addr - 64)) & 1;
    return false;
}

/* Property: scan count never exceeds I2C_SCAN_MAX_DEVICES */
PROPERTY(count_bounded, 10000) {
    g_ack_mask_lo = ARBI_UINT32(seed);
    g_ack_mask_mi = ARBI_UINT32(seed);
    g_ack_mask_hi = ARBI_UINT32(seed);

    i2c_scan_result_t result;
    memset(&result, 0, sizeof(result));
    i2c_scan(mock_probe, NULL, &result);
    PROP_ASSERT(result.count <= I2C_SCAN_MAX_DEVICES);
}

/* Property: scan_range returns only addresses within range */
PROPERTY(range_bounded, 10000) {
    g_ack_mask_lo = ARBI_UINT32(seed);
    g_ack_mask_mi = ARBI_UINT32(seed);
    g_ack_mask_hi = ARBI_UINT32(seed);

    uint8_t start = (uint8_t)ARBI_RANGE(seed, I2C_SCAN_ADDR_MIN, I2C_SCAN_ADDR_MAX);
    uint8_t end = (uint8_t)ARBI_RANGE(seed, start, I2C_SCAN_ADDR_MAX);

    i2c_scan_result_t result;
    memset(&result, 0, sizeof(result));
    i2c_scan_range(mock_probe, NULL, start, end, &result);

    for (uint8_t i = 0; i < result.count; i++) {
        PROP_ASSERT(result.addresses[i] >= start);
        PROP_ASSERT(result.addresses[i] <= end);
    }
}

/* Property: identify never crashes on any address */
PROPERTY(identify_no_crash, 5000) {
    (void)prop__seed_orig; (void)prop__iter;
    uint8_t addr = ARBI_UINT8(seed);
    const char *name = i2c_scan_identify(addr);
    /* name is either NULL or a valid pointer — just don't crash */
    (void)name;
}

TEST_MAIN("i2c_scan properties") {
    RUN(count_bounded);
    RUN(range_bounded);
    RUN(identify_no_crash);
}
TEST_MAIN_END()
