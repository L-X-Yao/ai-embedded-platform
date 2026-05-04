// SPDX-License-Identifier: MIT
/*
 * prop_hmc5883l.c — property-based tests for HMC5883L magnetometer
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "hmc5883l.h"

static const hmc5883l_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* Plant ID registers: 0x0A='H', 0x0B='4', 0x0C='3' */
static void plant_id(void)
{
    mock_regs[0x0A] = 'H';
    mock_regs[0x0B] = '4';
    mock_regs[0x0C] = '3';
}

PROPERTY(read_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    plant_id();

    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &prop_i2c, &cfg);

    hmc5883l_data_t data;
    hmc5883l_read_gauss(&dev, &data);
}

PROPERTY(gauss_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    plant_id();

    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = {
        .gain = (hmc5883l_gain_t)(ARBI_UINT8(seed) % 8),
        .rate = (hmc5883l_rate_t)(ARBI_UINT8(seed) % 7),
    };
    hmc5883l_err_t err = hmc5883l_init(&dev, &prop_i2c, &cfg);
    if (err != HMC5883L_OK) return;

    hmc5883l_data_t data;
    err = hmc5883l_read_gauss(&dev, &data);
    if (err != HMC5883L_OK) return;

    PROP_ASSERT(data.x_gauss == data.x_gauss);
    PROP_ASSERT(data.y_gauss == data.y_gauss);
    PROP_ASSERT(data.z_gauss == data.z_gauss);
}

TEST_MAIN("prop_hmc5883l")
    RUN(read_no_crash);
    RUN(gauss_finite);
TEST_MAIN_END()
