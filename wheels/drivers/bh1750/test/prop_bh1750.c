// SPDX-License-Identifier: MIT
/*
 * prop_bh1750.c — property-based tests for BH1750 ambient light sensor
 *
 * BH1750 uses command-based I2C (write/read), not register-based.
 */

#include "test_harness.h"
#include "property_test.h"
#include "bh1750.h"

/* --- command-based I2C mock --- */

static uint8_t mock_response[4];
static int     mock_fail_next;

static bool mock_write(void *ctx, uint8_t dev_addr,
                       const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)data; (void)len;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    return true;
}

static bool mock_read(void *ctx, uint8_t dev_addr,
                      uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    for (uint16_t i = 0; i < len; i++)
        data[i] = (i < sizeof(mock_response)) ? mock_response[i] : 0xFF;
    return true;
}

static void mock_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static const bh1750_i2c_t prop_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

static void mock_reset(void)
{
    memset(mock_response, 0, sizeof(mock_response));
    mock_fail_next = 0;
}

PROPERTY(init_no_crash, 10000)
{
    mock_reset();
    bh1750_t dev;
    bh1750_cfg_t cfg = {
        .addr = BH1750_ADDR_LOW,
        .mode = BH1750_CONT_H_RES,
    };
    bh1750_init(&dev, &prop_i2c, &cfg);
}

PROPERTY(read_no_crash, 5000)
{
    mock_reset();
    mock_response[0] = ARBI_UINT8(seed);
    mock_response[1] = ARBI_UINT8(seed);

    bh1750_t dev;
    bh1750_cfg_t cfg = {
        .addr = BH1750_ADDR_LOW,
        .mode = BH1750_CONT_H_RES,
    };
    bh1750_init(&dev, &prop_i2c, &cfg);

    float lux;
    bh1750_err_t err = bh1750_read_lux(&dev, &lux);
    if (err != BH1750_OK) return;

    PROP_ASSERT(lux == lux); /* not NaN */
    PROP_ASSERT(lux >= 0.0f);
}

TEST_MAIN("prop_bh1750")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
