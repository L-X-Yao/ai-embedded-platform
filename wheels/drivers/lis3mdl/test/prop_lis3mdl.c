// SPDX-License-Identifier: MIT
/*
 * prop_lis3mdl.c — property-based tests for LIS3MDL magnetometer
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "lis3mdl.h"

static const lis3mdl_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* Plant WHO_AM_I register */
static void plant_id(void)
{
    mock_regs[0x0F] = 0x3D;
}

PROPERTY(read_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    plant_id();

    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = {
        .range = (lis3mdl_range_t)(ARBI_UINT8(seed) % 4),
        .addr  = LIS3MDL_DEV_ADDR,
    };
    lis3mdl_init(&dev, &prop_i2c, &cfg);

    lis3mdl_data_t data;
    lis3mdl_read(&dev, &data);
}

PROPERTY(ut_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    plant_id();

    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = {
        .range = (lis3mdl_range_t)(ARBI_UINT8(seed) % 4),
        .addr  = LIS3MDL_DEV_ADDR,
    };
    lis3mdl_err_t err = lis3mdl_init(&dev, &prop_i2c, &cfg);
    if (err != LIS3MDL_OK) return;

    lis3mdl_data_t data;
    err = lis3mdl_read(&dev, &data);
    if (err != LIS3MDL_OK) return;

    PROP_ASSERT(data.x_ut == data.x_ut);
    PROP_ASSERT(data.y_ut == data.y_ut);
    PROP_ASSERT(data.z_ut == data.z_ut);
}

TEST_MAIN("prop_lis3mdl")
    RUN(read_no_crash);
    RUN(ut_finite);
TEST_MAIN_END()
