// SPDX-License-Identifier: MIT
/*
 * prop_ds3231.c — property-based tests for DS3231 RTC
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "ds3231.h"

static const ds3231_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

PROPERTY(init_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    ds3231_t dev;
    ds3231_init(&dev, &prop_i2c);
}

PROPERTY(read_write_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    ds3231_t dev;
    ds3231_init(&dev, &prop_i2c);

    ds3231_time_t time;
    ds3231_get_time(&dev, &time);

    float temp;
    ds3231_get_temperature(&dev, &temp);

    ds3231_time_t wtime = {
        .year   = ARBI_UINT8(seed) % 100,
        .month  = (uint8_t)(1 + ARBI_UINT8(seed) % 12),
        .day    = (uint8_t)(1 + ARBI_UINT8(seed) % 28),
        .weekday = (uint8_t)(1 + ARBI_UINT8(seed) % 7),
        .hour   = ARBI_UINT8(seed) % 24,
        .minute = ARBI_UINT8(seed) % 60,
        .second = ARBI_UINT8(seed) % 60,
    };
    ds3231_set_time(&dev, &wtime);
}

PROPERTY(temp_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    ds3231_t dev;
    ds3231_init(&dev, &prop_i2c);

    float temp;
    ds3231_err_t err = ds3231_get_temperature(&dev, &temp);
    if (err != DS3231_OK) return;

    PROP_ASSERT(temp == temp); /* not NaN */
}

TEST_MAIN("prop_ds3231")
    RUN(init_no_crash);
    RUN(read_write_no_crash);
    RUN(temp_finite);
TEST_MAIN_END()
