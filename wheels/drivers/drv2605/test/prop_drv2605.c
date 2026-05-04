// SPDX-License-Identifier: MIT
/*
 * prop_drv2605.c — property-based tests for DRV2605 haptic driver
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "drv2605.h"

static const drv2605_i2c_t prop_i2c = {
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
    /* Status register: device ID in bits [7:5] = 7 (DRV2605L) */
    mock_regs[0x00] = (uint8_t)(DRV2605_DEVID_2605L << 5);

    drv2605_t dev;
    drv2605_init(&dev, &prop_i2c);
}

PROPERTY(waveform_no_crash, 5000)
{
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x00] = (uint8_t)(DRV2605_DEVID_2605L << 5);

    drv2605_t dev;
    drv2605_err_t err = drv2605_init(&dev, &prop_i2c);
    if (err != DRV2605_OK) return;

    uint8_t slot = ARBI_UINT8(seed) % DRV2605_WAVEFORM_SLOTS;
    uint8_t effect = ARBI_UINT8(seed);
    drv2605_set_waveform(&dev, slot, effect);
    drv2605_set_realtime_value(&dev, ARBI_UINT8(seed));
    drv2605_go(&dev);
    drv2605_stop(&dev);
}

TEST_MAIN("prop_drv2605")
    RUN(init_no_crash);
    RUN(waveform_no_crash);
TEST_MAIN_END()
