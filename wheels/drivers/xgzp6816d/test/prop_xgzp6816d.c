// SPDX-License-Identifier: MIT
/*
 * prop_xgzp6816d.c — property-based tests for XGZP6816D pressure sensor
 *
 * XGZP6816D uses command-based I2C (write/read), not register-based.
 */

#include "test_harness.h"
#include "property_test.h"
#include "xgzp6816d.h"

/* --- command-based I2C mock --- */

static uint8_t mock_response[8];
static int     mock_response_len;
static int     mock_fail_next;

static bool mock_write(void *ctx, uint8_t dev_addr,
                       const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr; (void)buf; (void)len;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    return true;
}

static bool mock_read(void *ctx, uint8_t dev_addr,
                      uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    for (uint16_t i = 0; i < len; i++)
        buf[i] = (i < (uint16_t)mock_response_len) ? mock_response[i] : 0x00;
    return true;
}

static const xgzp6816d_i2c_t prop_i2c = {
    .write = mock_write,
    .read  = mock_read,
    .ctx   = NULL,
};

static void mock_reset(void)
{
    memset(mock_response, 0, sizeof(mock_response));
    mock_response_len = 0;
    mock_fail_next = 0;
}

PROPERTY(probe_no_crash, 10000)
{
    mock_reset();
    xgzp6816d_probe(&prop_i2c);
}

PROPERTY(read_no_crash, 5000)
{
    mock_reset();
    /* Plant random 5-byte response: pressure[2:0], temp[1:0] */
    for (int i = 0; i < 5; i++)
        mock_response[i] = ARBI_UINT8(seed);
    mock_response_len = 5;

    xgzp6816d_trigger(&prop_i2c);

    xgzp6816d_data_t data;
    xgzp6816d_err_t err = xgzp6816d_read(&prop_i2c, &data);
    if (err != XGZP6816D_OK) return;

    PROP_ASSERT(data.pressure_pa == data.pressure_pa); /* not NaN */
    PROP_ASSERT(data.temperature_c == data.temperature_c);
}

TEST_MAIN("prop_xgzp6816d")
    RUN(probe_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
