// SPDX-License-Identifier: MIT
/*
 * test_hw_model_tca9548a.c — tests using TCA9548A hardware register model
 *
 * Validates TCA9548A driver against a single-register model.
 * Tests channel selection, multi-channel bitmask, disable-all,
 * cache optimization (skip redundant writes), and read-back.
 */

#include "test_harness.h"
#include "hw_model_tca9548a.h"
#include "../tca9548a.h"

static tca9548a_model_t model;

static const tca9548a_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = tca9548a_model_delay,
    .ctx       = &model.hw,
};

static void setup(void)
{
    tca9548a_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_disables_all_channels)
{
    setup();
    tca9548a_t dev;
    tca9548a_err_t err = tca9548a_init(&dev, &model_i2c, 0x70);
    ASSERT_EQ(err, TCA9548A_OK);
    /* Control register should be 0x00 */
    ASSERT_EQ(model.hw.regs[TCA9548A_REG_CTRL], 0x00);
}

TEST(test_init_rejects_invalid_addr)
{
    setup();
    tca9548a_t dev;
    ASSERT_EQ(tca9548a_init(&dev, &model_i2c, 0x6F), TCA9548A_ERR_PARAM);
    ASSERT_EQ(tca9548a_init(&dev, &model_i2c, 0x78), TCA9548A_ERR_PARAM);
}

/* --- single channel select --------------------------------------------- */

TEST(test_select_channel_0)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, 0x70);

    tca9548a_err_t err = tca9548a_select(&dev, 0);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.hw.regs[TCA9548A_REG_CTRL], 0x01);
}

TEST(test_select_channel_7)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, 0x70);

    tca9548a_err_t err = tca9548a_select(&dev, 7);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.hw.regs[TCA9548A_REG_CTRL], 0x80);
}

TEST(test_select_channel_out_of_range)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, 0x70);
    ASSERT_EQ(tca9548a_select(&dev, 8), TCA9548A_ERR_PARAM);
}

/* --- multi-channel ----------------------------------------------------- */

TEST(test_select_multi)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, 0x70);

    tca9548a_err_t err = tca9548a_select_multi(&dev, 0x05); /* ch0 + ch2 */
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.hw.regs[TCA9548A_REG_CTRL], 0x05);
}

/* --- disable all ------------------------------------------------------- */

TEST(test_disable_all)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, 0x70);

    tca9548a_select(&dev, 3);
    ASSERT_EQ(model.hw.regs[TCA9548A_REG_CTRL], 0x08);

    tca9548a_err_t err = tca9548a_disable_all(&dev);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.hw.regs[TCA9548A_REG_CTRL], 0x00);
}

/* --- cache optimization ------------------------------------------------ */

TEST(test_cache_skips_redundant_write)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, 0x70);

    tca9548a_select(&dev, 2);
    int writes_after_first = model.write_count;

    /* Select same channel again — should skip I2C write */
    tca9548a_select(&dev, 2);
    ASSERT_EQ(model.write_count, writes_after_first);
}

TEST(test_cache_writes_on_change)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, 0x70);

    tca9548a_select(&dev, 2);
    int writes_after_first = model.write_count;

    tca9548a_select(&dev, 5);
    ASSERT_TRUE(model.write_count > writes_after_first);
    ASSERT_EQ(model.hw.regs[TCA9548A_REG_CTRL], 0x20);
}

/* --- read-back --------------------------------------------------------- */

TEST(test_get_channel_reads_register)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, 0x70);

    tca9548a_select_multi(&dev, 0xA5);

    uint8_t mask = 0;
    tca9548a_err_t err = tca9548a_get_channel(&dev, &mask);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(mask, 0xA5);
}

/* --- register protection ----------------------------------------------- */

TEST(test_nonexistent_registers_readonly)
{
    setup();
    /* Registers 1..255 are non-existent and should be read-only */
    hw_model__write_reg(&model.hw, 0x01, 0x42);
    ASSERT_EQ(model.hw.regs[0x01], 0xFF);
}

TEST_MAIN("tca9548a hardware model")
    RUN(test_init_disables_all_channels);
    RUN(test_init_rejects_invalid_addr);
    RUN(test_select_channel_0);
    RUN(test_select_channel_7);
    RUN(test_select_channel_out_of_range);
    RUN(test_select_multi);
    RUN(test_disable_all);
    RUN(test_cache_skips_redundant_write);
    RUN(test_cache_writes_on_change);
    RUN(test_get_channel_reads_register);
    RUN(test_nonexistent_registers_readonly);
TEST_MAIN_END()
