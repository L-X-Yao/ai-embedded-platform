// SPDX-License-Identifier: MIT
/*
 * prop_pca9685.c — property-based tests for PCA9685 PWM driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "pca9685.h"

static const pca9685_i2c_t prop_i2c = {
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

    pca9685_t dev;
    uint16_t freq = (uint16_t)(PCA9685_FREQ_MIN + ARBI_UINT16(seed) %
                    (PCA9685_FREQ_MAX - PCA9685_FREQ_MIN + 1));
    pca9685_init(&dev, &prop_i2c, PCA9685_ADDR_DEFAULT, freq);
}

PROPERTY(set_pwm_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);

    pca9685_t dev;
    pca9685_init(&dev, &prop_i2c, PCA9685_ADDR_DEFAULT, 50);

    uint8_t ch = ARBI_UINT8(seed) % 16;
    uint16_t on  = ARBI_UINT16(seed) & 0x0FFF;
    uint16_t off = ARBI_UINT16(seed) & 0x0FFF;
    pca9685_set_pwm(&dev, ch, on, off);

    float duty = (float)(ARBI_UINT8(seed) % 101);
    pca9685_set_duty(&dev, ch, duty);
}

TEST_MAIN("prop_pca9685")
    RUN(init_no_crash);
    RUN(set_pwm_no_crash);
TEST_MAIN_END()
