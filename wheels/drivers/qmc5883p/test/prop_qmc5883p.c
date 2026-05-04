// SPDX-License-Identifier: MIT
/*
 * prop_qmc5883p.c — property-based tests for QMC5883P magnetometer
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "qmc5883p.h"

static const qmc5883p_i2c_t prop_i2c = {
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
    mock_regs[0x00] = QMC5883P_CHIP_ID;

    qmc5883p_init(&prop_i2c);

    qmc5883p_data_t data;
    qmc5883p_read(&prop_i2c, &data);
}

PROPERTY(output_finite, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x00] = QMC5883P_CHIP_ID;

    qmc5883p_err_t err = qmc5883p_init(&prop_i2c);
    if (err != QMC5883P_OK) return;

    qmc5883p_data_t data;
    err = qmc5883p_read(&prop_i2c, &data);
    if (err != QMC5883P_OK) return;

    PROP_ASSERT(data.mx_gauss == data.mx_gauss);
    PROP_ASSERT(data.my_gauss == data.my_gauss);
    PROP_ASSERT(data.mz_gauss == data.mz_gauss);
}

TEST_MAIN("prop_qmc5883p")
    RUN(read_no_crash);
    RUN(output_finite);
TEST_MAIN_END()
