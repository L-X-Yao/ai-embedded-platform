// SPDX-License-Identifier: MIT
/*
 * prop_tmp102.c — property-based tests for TMP102 temperature sensor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "tmp102.h"

static const tmp102_i2c_t prop_i2c = {
    .write_reg  = mock_i2c_write_reg,
    .write_regs = mock_i2c_write_regs,
    .read_regs  = mock_i2c_read_regs,
    .delay_ms   = mock_i2c_delay_ms,
    .ctx        = NULL,
};

PROPERTY(read_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    tmp102_t dev;
    tmp102_cfg_t cfg = { .addr = TMP102_ADDR_GND };
    tmp102_init(&dev, &prop_i2c, &cfg);

    float temp;
    tmp102_read_temp(&dev, &temp);
}

PROPERTY(temp_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    tmp102_t dev;
    tmp102_cfg_t cfg = { .addr = TMP102_ADDR_GND };
    tmp102_err_t err = tmp102_init(&dev, &prop_i2c, &cfg);
    if (err != TMP102_OK) return;

    float temp;
    err = tmp102_read_temp(&dev, &temp);
    if (err != TMP102_OK) return;

    PROP_ASSERT(temp == temp); /* not NaN */
    /* TMP102 range: -40 to 125C; with random regs allow wider margin */
    PROP_ASSERT(temp > -300.0f && temp < 300.0f);
}

TEST_MAIN("prop_tmp102")
    RUN(read_no_crash);
    RUN(temp_finite);
TEST_MAIN_END()
