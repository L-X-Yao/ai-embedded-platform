// SPDX-License-Identifier: MIT
/*
 * prop_si7021.c — property-based tests for Si7021 temp/humidity sensor
 *
 * Si7021 uses command-based I2C (write/read), not register-based.
 */

#include "test_harness.h"
#include "property_test.h"
#include "si7021.h"

/* --- command-based I2C mock --- */

static uint8_t mock_response[8];
static int     mock_response_len;
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
        data[i] = (i < (uint16_t)mock_response_len) ? mock_response[i] : 0xFF;
    return true;
}

static void mock_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static const si7021_i2c_t prop_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay,
    .ctx      = NULL,
};

static void mock_reset(void)
{
    memset(mock_response, 0, sizeof(mock_response));
    mock_response_len = 0;
    mock_fail_next = 0;
}

PROPERTY(init_no_crash, 10000)
{
    mock_reset();
    si7021_t dev;
    si7021_init(&dev, &prop_i2c);
}

PROPERTY(read_no_crash, 5000)
{
    mock_reset();
    /* Plant random 3-byte responses (MSB, LSB, CRC) */
    mock_response[0] = ARBI_UINT8(seed);
    mock_response[1] = ARBI_UINT8(seed);
    mock_response[2] = ARBI_UINT8(seed);
    mock_response_len = 3;

    si7021_t dev;
    si7021_init(&dev, &prop_i2c);

    float rh, temp;
    si7021_read_humidity(&dev, &rh);
    si7021_read_temperature(&dev, &temp);
}

PROPERTY(crc_pure, 5000)
{
    /* CRC function must not crash on arbitrary data */
    uint8_t buf[8];
    size_t len = ARBI_SIZE(seed, 8);
    ARBI_FILL(seed, buf, len);
    uint8_t c = si7021_crc8(buf, (uint16_t)len);
    (void)c;
}

TEST_MAIN("prop_si7021")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(crc_pure);
TEST_MAIN_END()
