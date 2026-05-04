// SPDX-License-Identifier: MIT
/*
 * prop_vl53l0x.c — property-based tests for VL53L0X ToF sensor
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "vl53l0x.h"

static const vl53l0x_i2c_t prop_i2c = {
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
    mock_regs[0xC0] = VL53L0X_MODEL_ID; /* model ID register */

    vl53l0x_t dev;
    vl53l0x_cfg_t cfg = { .addr = VL53L0X_ADDR_DEFAULT };
    vl53l0x_init(&dev, &prop_i2c, &cfg);
}

PROPERTY(read_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0xC0] = VL53L0X_MODEL_ID;
    /* Fake "measurement ready" so read does not infinite-loop */
    mock_regs[0x13] = 0x10;

    vl53l0x_t dev;
    vl53l0x_cfg_t cfg = { .addr = VL53L0X_ADDR_DEFAULT };
    vl53l0x_err_t err = vl53l0x_init(&dev, &prop_i2c, &cfg);
    if (err != VL53L0X_OK) return;

    uint16_t range;
    vl53l0x_read_range_mm(&dev, &range);
}

TEST_MAIN("prop_vl53l0x")
    RUN(init_no_crash);
    RUN(read_no_crash);
TEST_MAIN_END()
