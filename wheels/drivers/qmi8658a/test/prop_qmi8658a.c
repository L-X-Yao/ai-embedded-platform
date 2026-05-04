// SPDX-License-Identifier: MIT
/*
 * prop_qmi8658a.c — property-based tests for QMI8658A IMU driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "qmi8658a.h"

static const qmi8658a_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

PROPERTY(read_no_crash, 10000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x00] = 0x05; /* WHO_AM_I */

    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_init(&dev, &prop_i2c, &cfg);

    qmi8658a_data_t data;
    qmi8658a_read(&dev, &data);
}

PROPERTY(output_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x00] = 0x05;

    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = qmi8658a_default_cfg();
    qmi8658a_err_t err = qmi8658a_init(&dev, &prop_i2c, &cfg);
    if (err != QMI8658A_OK) return;

    qmi8658a_data_t data;
    err = qmi8658a_read(&dev, &data);
    if (err != QMI8658A_OK) return;

    PROP_ASSERT(data.ax_g == data.ax_g);
    PROP_ASSERT(data.ay_g == data.ay_g);
    PROP_ASSERT(data.az_g == data.az_g);
    PROP_ASSERT(data.gx_dps == data.gx_dps);
    PROP_ASSERT(data.gy_dps == data.gy_dps);
    PROP_ASSERT(data.gz_dps == data.gz_dps);
}

PROPERTY(random_cfg_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x00] = 0x05;

    qmi8658a_t dev;
    qmi8658a_cfg_t cfg = {
        .acc_range = (qmi8658a_acc_range_t)(ARBI_UINT8(seed) % 4),
        .gyr_range = (qmi8658a_gyr_range_t)(ARBI_UINT8(seed) % 8),
        .acc_odr   = (qmi8658a_odr_t)(ARBI_UINT8(seed) % 9),
        .gyr_odr   = (qmi8658a_odr_t)(ARBI_UINT8(seed) % 9),
    };
    qmi8658a_init(&dev, &prop_i2c, &cfg);
}

TEST_MAIN("prop_qmi8658a")
    RUN(read_no_crash);
    RUN(output_finite);
    RUN(random_cfg_no_crash);
TEST_MAIN_END()
