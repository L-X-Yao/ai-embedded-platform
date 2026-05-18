// SPDX-License-Identifier: MIT
/*
 * prop_soft_spi.c --- property-based tests for soft_spi
 *
 * Build: add to test/CMakeLists.txt or compile standalone:
 *   gcc -std=c99 -I.. -I../../../../tooling/harness prop_soft_spi.c ../soft_spi.c -o prop_soft_spi
 */

#include "test_harness.h"
#include "property_test.h"
#include "soft_spi.h"
#include <string.h>

/* ---------- mock GPIO ---------- */

static uint8_t g_miso_bits[4096];
static int g_miso_idx;
static int g_miso_len;
static void m_sck_low(void *ctx) { (void)ctx; }
static void m_sck_high(void *ctx) { (void)ctx; }
static void m_mosi_low(void *ctx) { (void)ctx; }
static void m_mosi_high(void *ctx) { (void)ctx; }
static bool m_miso_read(void *ctx) {
    (void)ctx;
    if (g_miso_idx < g_miso_len)
        return g_miso_bits[g_miso_idx++] != 0;
    return false;
}
static void m_delay(void *ctx, uint16_t us) { (void)ctx; (void)us; }

static const soft_spi_gpio_t g_gpio = {
    .sck_low = m_sck_low, .sck_high = m_sck_high,
    .mosi_low = m_mosi_low, .mosi_high = m_mosi_high,
    .miso_read = m_miso_read, .delay_us = m_delay, .ctx = NULL,
};

static void mock_reset_prop(void) {
    g_miso_idx = 0;
    g_miso_len = 0;
    memset(g_miso_bits, 0, sizeof(g_miso_bits));
}

/* Property: xfer never crashes regardless of mode/lsb/byte value */
PROPERTY(xfer_never_crashes, 10000) {
    soft_spi_t dev;
    mock_reset_prop();
    uint8_t mode = (uint8_t)(ARBI_UINT8(seed) & 3);
    bool lsb = (ARBI_UINT8(seed) & 1) != 0;
    soft_spi_cfg_t cfg;
    cfg.mode = mode;
    cfg.lsb_first = lsb;
    soft_spi_init(&dev, &g_gpio, &cfg);

    uint8_t tx = ARBI_UINT8(seed);

    /* Random MISO bits */
    for (int i = 0; i < 8; i++)
        g_miso_bits[i] = ARBI_UINT8(seed) & 1;
    g_miso_len = 8;

    uint8_t rx = soft_spi_xfer(&dev, tx);
    /* xfer always returns a byte -- just check no crash */
    (void)rx;
    PROP_ASSERT(1);
}

/* Property: write never crashes regardless of data and mode */
PROPERTY(write_never_crashes, 10000) {
    soft_spi_t dev;
    mock_reset_prop();
    uint8_t mode = (uint8_t)(ARBI_UINT8(seed) & 3);
    soft_spi_cfg_t cfg;
    cfg.mode = mode;
    cfg.lsb_first = (ARBI_UINT8(seed) & 1) != 0;
    soft_spi_init(&dev, &g_gpio, &cfg);

    uint8_t data[32];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 0, 32);
    ARBI_FILL(seed, data, len);

    /* Random MISO */
    int total = len * 8;
    for (int i = 0; i < total && i < (int)sizeof(g_miso_bits); i++)
        g_miso_bits[i] = ARBI_UINT8(seed) & 1;
    g_miso_len = total;

    soft_spi_err_t err = soft_spi_write(&dev, data, len);
    PROP_ASSERT(err == SOFT_SPI_OK);
}

/* Property: read never crashes regardless of MISO pattern */
PROPERTY(read_never_crashes, 10000) {
    soft_spi_t dev;
    mock_reset_prop();
    uint8_t mode = (uint8_t)(ARBI_UINT8(seed) & 3);
    soft_spi_cfg_t cfg;
    cfg.mode = mode;
    cfg.lsb_first = (ARBI_UINT8(seed) & 1) != 0;
    soft_spi_init(&dev, &g_gpio, &cfg);

    uint8_t buf[32];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 1, 32);

    int total = len * 8;
    for (int i = 0; i < total && i < (int)sizeof(g_miso_bits); i++)
        g_miso_bits[i] = ARBI_UINT8(seed) & 1;
    g_miso_len = total;

    soft_spi_err_t err = soft_spi_read(&dev, buf, len);
    PROP_ASSERT(err == SOFT_SPI_OK);
}

/* Property: transfer = write + read simultaneously, no crash */
PROPERTY(transfer_never_crashes, 10000) {
    soft_spi_t dev;
    mock_reset_prop();
    uint8_t mode = (uint8_t)(ARBI_UINT8(seed) & 3);
    soft_spi_cfg_t cfg;
    cfg.mode = mode;
    cfg.lsb_first = (ARBI_UINT8(seed) & 1) != 0;
    soft_spi_init(&dev, &g_gpio, &cfg);

    uint8_t tx[32], rx[32];
    uint16_t len = (uint16_t)ARBI_RANGE(seed, 1, 32);
    ARBI_FILL(seed, tx, len);

    int total = len * 8;
    for (int i = 0; i < total && i < (int)sizeof(g_miso_bits); i++)
        g_miso_bits[i] = ARBI_UINT8(seed) & 1;
    g_miso_len = total;

    soft_spi_err_t err = soft_spi_transfer(&dev, tx, rx, len);
    PROP_ASSERT(err == SOFT_SPI_OK);
}

/* Property: init rejects mode > 3 */
PROPERTY(init_rejects_bad_mode, 1000) {
    soft_spi_t dev;
    uint8_t mode = (uint8_t)ARBI_RANGE(seed, 4, 255);
    soft_spi_cfg_t cfg;
    cfg.mode = mode;
    cfg.lsb_first = false;
    PROP_ASSERT_EQ(SOFT_SPI_ERR_PARAM, soft_spi_init(&dev, &g_gpio, &cfg));
}

TEST_MAIN("soft_spi properties")
    RUN(xfer_never_crashes);
    RUN(write_never_crashes);
    RUN(read_never_crashes);
    RUN(transfer_never_crashes);
    RUN(init_rejects_bad_mode);
TEST_MAIN_END()
