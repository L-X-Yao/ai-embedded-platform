// SPDX-License-Identifier: MIT
/*
 * prop_apds9960.c — property-based tests for APDS-9960 gesture/color sensor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "apds9960.h"

static const apds9960_i2c_t prop_i2c = {
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
    mock_regs[0x92] = 0xAB; /* chip ID register */

    apds9960_t dev;
    apds9960_init(&dev, &prop_i2c);
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x92] = 0xAB;

    apds9960_t dev;
    apds9960_err_t err = apds9960_init(&dev, &prop_i2c);
    if (err != APDS9960_OK) return;

    apds9960_enable_proximity(&dev);
    uint8_t prox;
    apds9960_read_proximity(&dev, &prox);

    apds9960_enable_color(&dev);
    apds9960_color_t color;
    apds9960_read_color(&dev, &color);
}

PROPERTY(gesture_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x92] = 0xAB;
    /* Set GFLVL (gesture FIFO level) to 0 so gesture read exits quickly */
    mock_regs[0xAE] = 0;

    apds9960_t dev;
    apds9960_err_t err = apds9960_init(&dev, &prop_i2c);
    if (err != APDS9960_OK) return;

    apds9960_enable_gesture(&dev);
    apds9960_gesture_t g;
    apds9960_read_gesture(&dev, &g);
}

TEST_MAIN("prop_apds9960")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(gesture_no_crash);
TEST_MAIN_END()
