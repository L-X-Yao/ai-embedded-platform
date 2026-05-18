// SPDX-License-Identifier: MIT
/*
 * prop_rc522.c --- property-based tests for rc522
 *
 * Build: gcc -std=c99 -Wall -Wextra -I.. -I../../../../tooling/harness prop_rc522.c ../rc522.c -o prop_rc522
 */

#include "test_harness.h"
#include "property_test.h"
#include "../rc522.h"
#include <string.h>

/* ---------- inline SPI mock (rc522 uses a complex SPI protocol) ---------- */

#define MOCK_REG_COUNT 64
static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_op_count_prop;
static uint8_t mock_cs_state_prop;
static int     mock_byte_idx_prop;
static bool    mock_is_read_prop;
static uint8_t mock_addr_latch_prop;

static void prop_mock_reset(void) {
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_op_count_prop  = 0;
    mock_cs_state_prop  = 1;
    mock_byte_idx_prop  = 0;
    mock_is_read_prop   = false;
    mock_addr_latch_prop = 0;
    /* Set version register */
    mock_regs[0x37] = 0x92;
    mock_regs[0x06] = 0x00;
}

static void prop_cs_low(void *ctx) {
    (void)ctx;
    mock_cs_state_prop = 0;
    mock_byte_idx_prop = 0;
}

static void prop_cs_high(void *ctx) { (void)ctx; mock_cs_state_prop = 1; }

static uint8_t prop_xfer(void *ctx, uint8_t tx) {
    (void)ctx;
    mock_op_count_prop++;

    if (mock_byte_idx_prop == 0) {
        /* Address byte: bit7=1=read, bits6:1=addr, bit0=0 */
        mock_is_read_prop = (tx & 0x80u) != 0;
        mock_addr_latch_prop = (uint8_t)((tx >> 1) & 0x3Fu);
        mock_byte_idx_prop++;
        return 0x00;
    }

    if (mock_is_read_prop) {
        uint8_t reg = mock_addr_latch_prop;
        mock_byte_idx_prop++;
        return (reg < MOCK_REG_COUNT) ? mock_regs[reg] : 0x00;
    } else {
        uint8_t reg = mock_addr_latch_prop;
        if (reg < MOCK_REG_COUNT) mock_regs[reg] = tx;
        mock_byte_idx_prop++;
        return 0x00;
    }
}

static void prop_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static const rc522_spi_t prop_spi = {
    .cs_low   = prop_cs_low,
    .cs_high  = prop_cs_high,
    .xfer     = prop_xfer,
    .delay_ms = prop_delay,
    .ctx      = NULL,
};

/* Property: init does not crash with random register contents */
PROPERTY(init_no_crash_random_regs, 5000) {
    prop_mock_reset();

    /* Randomize all registers except version */
    for (int i = 0; i < MOCK_REG_COUNT; i++) {
        if (i == 0x37) continue; /* keep version valid */
        mock_regs[i] = ARBI_UINT8(seed);
    }

    rc522_t dev;
    memset(&dev, 0, sizeof(dev));
    rc522_err_t rc = rc522_init(&dev, &prop_spi);
    PROP_ASSERT_EQ(rc, RC522_OK);
    /* No crash is success */
}

/* Property: version is always 0x91 or 0x92 when init succeeds */
PROPERTY(version_valid, 5000) {
    prop_mock_reset();

    /* Randomly pick v1.0 or v2.0 */
    mock_regs[0x37] = (ARBI_UINT8(seed) & 1) ? 0x91 : 0x92;

    rc522_t dev;
    memset(&dev, 0, sizeof(dev));
    rc522_err_t rc = rc522_init(&dev, &prop_spi);
    PROP_ASSERT_EQ(rc, RC522_OK);

    uint8_t ver = rc522_get_version(&dev);
    PROP_ASSERT(ver == RC522_VERSION_1_0 || ver == RC522_VERSION_2_0);
}

/* Property: bad version register causes init to fail */
PROPERTY(bad_version_rejected, 5000) {
    prop_mock_reset();

    uint8_t bad_ver = ARBI_UINT8(seed);
    /* Ensure it is NOT a valid version */
    if (bad_ver == 0x91 || bad_ver == 0x92) bad_ver = 0x00;
    mock_regs[0x37] = bad_ver;

    rc522_t dev;
    memset(&dev, 0, sizeof(dev));
    rc522_err_t rc = rc522_init(&dev, &prop_spi);
    PROP_ASSERT(rc != RC522_OK);
}

/* Property: get_version returns consistent value post-init */
PROPERTY(version_deterministic, 1000) {
    prop_mock_reset();
    rc522_t dev;
    memset(&dev, 0, sizeof(dev));
    rc522_init(&dev, &prop_spi);

    uint8_t v1 = rc522_get_version(&dev);
    uint8_t v2 = rc522_get_version(&dev);
    PROP_ASSERT_EQ(v1, v2);
}

TEST_MAIN("rc522 properties") {
    RUN(init_no_crash_random_regs);
    RUN(version_valid);
    RUN(bad_version_rejected);
    RUN(version_deterministic);
}
TEST_MAIN_END()
