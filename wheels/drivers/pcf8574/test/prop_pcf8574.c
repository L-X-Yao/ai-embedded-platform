// SPDX-License-Identifier: MIT
/*
 * prop_pcf8574.c — property-based tests for PCF8574 I2C GPIO expander
 *
 * PCF8574 uses command-based I2C (write/read), not register-based.
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_time.h"
#include "pcf8574.h"

/* --- command-based I2C mock --- */

static uint8_t mock_output;
static int     mock_fail_next;

static bool mock_write(void *ctx, uint8_t dev_addr,
                       const uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    if (len > 0) mock_output = data[0];
    return true;
}

static bool mock_read(void *ctx, uint8_t dev_addr,
                      uint8_t *data, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_fail_next) { mock_fail_next = 0; return false; }
    for (uint16_t i = 0; i < len; i++)
        data[i] = mock_output;
    return true;
}

static const pcf8574_i2c_t prop_i2c = {
    .write    = mock_write,
    .read     = mock_read,
    .delay_ms = mock_delay_ms,
    .ctx      = NULL,
};

static void mock_reset(void)
{
    mock_output    = 0xFF;
    mock_fail_next = 0;
}

PROPERTY(init_no_crash, 10000)
{
    mock_reset();
    pcf8574_t dev;
    uint8_t addr = (uint8_t)(0x20 + (ARBI_UINT8(seed) % 8));
    pcf8574_init(&dev, &prop_i2c, addr);
}

PROPERTY(read_write_no_crash, 5000)
{
    mock_reset();
    pcf8574_t dev;
    pcf8574_init(&dev, &prop_i2c, 0x20);

    pcf8574_write(&dev, ARBI_UINT8(seed));

    uint8_t val;
    pcf8574_read(&dev, &val);

    uint8_t pin = ARBI_UINT8(seed) % 8;
    pcf8574_write_pin(&dev, pin, ARBI_UINT8(seed) & 1);

    bool level;
    pcf8574_read_pin(&dev, pin, &level);

    pcf8574_toggle_pin(&dev, pin);
}

TEST_MAIN("prop_pcf8574")
    RUN(init_no_crash);
    RUN(read_write_no_crash);
TEST_MAIN_END()
