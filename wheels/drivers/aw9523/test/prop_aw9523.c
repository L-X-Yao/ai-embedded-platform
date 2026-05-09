// SPDX-License-Identifier: MIT
/*
 * prop_aw9523.c — property-based tests for aw9523
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_aw9523.c ../aw9523.c -o prop_aw9523
 */

#include "test_harness.h"
#include "property_test.h"
#include "mock_i2c.h"
#include "aw9523.h"

static const aw9523_i2c_t prop_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ------------------------------------------------------------------ */

PROPERTY(init_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    for (int i = 0; i < 256; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    /* Set valid chip ID so init proceeds */
    mock_regs[0x10] = AW9523_CHIP_ID;

    aw9523_t dev;
    aw9523_init(&dev, &prop_i2c, AW9523_DEV_ADDR);
    /* Must not crash */
}

PROPERTY(write_read_pin_roundtrip, 5000)
{
    mock_i2c_reset();
    mock_regs[0x10] = AW9523_CHIP_ID;

    aw9523_t dev;
    aw9523_err_t err = aw9523_init(&dev, &prop_i2c, AW9523_DEV_ADDR);
    if (err != AW9523_OK) return;

    aw9523_port_t port = (ARBI_UINT8(seed) & 1) ? AW9523_PORT1 : AW9523_PORT0;
    uint8_t pin = ARBI_UINT8(seed) % 8;

    /* Set pin as output */
    err = aw9523_set_direction(&dev, port, pin, AW9523_DIR_OUTPUT);
    if (err != AW9523_OK) return;

    /* Write a random level */
    aw9523_level_t level = (ARBI_UINT8(seed) & 1) ? AW9523_PIN_HIGH : AW9523_PIN_LOW;
    err = aw9523_write_pin(&dev, port, pin, level);
    PROP_ASSERT(err == AW9523_OK);

    /* Verify the output register has the correct bit */
    uint8_t out_reg = (port == AW9523_PORT0) ? 0x02 : 0x03;
    uint8_t bit_val = (mock_regs[out_reg] >> pin) & 1;
    PROP_ASSERT_EQ(bit_val, (uint8_t)level);
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_i2c_reset();
    mock_regs[0x10] = AW9523_CHIP_ID;

    aw9523_t dev;
    aw9523_init(&dev, &prop_i2c, AW9523_DEV_ADDR);
    /* Second init */
    mock_i2c_reset();
    mock_regs[0x10] = AW9523_CHIP_ID;
    aw9523_init(&dev, &prop_i2c, AW9523_DEV_ADDR);
    /* Must not crash */
}

PROPERTY(led_current_any_value, 5000)
{
    mock_i2c_reset();
    mock_regs[0x10] = AW9523_CHIP_ID;

    aw9523_t dev;
    aw9523_err_t err = aw9523_init(&dev, &prop_i2c, AW9523_DEV_ADDR);
    if (err != AW9523_OK) return;

    aw9523_port_t port = (ARBI_UINT8(seed) & 1) ? AW9523_PORT1 : AW9523_PORT0;
    uint8_t pin = ARBI_UINT8(seed) % 8;
    uint8_t current = ARBI_UINT8(seed);

    err = aw9523_set_led_current(&dev, port, pin, current);
    PROP_ASSERT(err == AW9523_OK);
}

PROPERTY(port_write_read_consistent, 5000)
{
    mock_i2c_reset();
    mock_regs[0x10] = AW9523_CHIP_ID;

    aw9523_t dev;
    aw9523_err_t err = aw9523_init(&dev, &prop_i2c, AW9523_DEV_ADDR);
    if (err != AW9523_OK) return;

    aw9523_port_t port = (ARBI_UINT8(seed) & 1) ? AW9523_PORT1 : AW9523_PORT0;
    uint8_t val = ARBI_UINT8(seed);

    err = aw9523_write_port(&dev, port, val);
    PROP_ASSERT(err == AW9523_OK);

    /* Verify output register */
    uint8_t out_reg = (port == AW9523_PORT0) ? 0x02 : 0x03;
    PROP_ASSERT_EQ(mock_regs[out_reg], val);
}

TEST_MAIN("aw9523 properties")
    RUN(init_no_crash);
    RUN(write_read_pin_roundtrip);
    RUN(double_init_safe);
    RUN(led_current_any_value);
    RUN(port_write_read_consistent);
TEST_MAIN_END()
