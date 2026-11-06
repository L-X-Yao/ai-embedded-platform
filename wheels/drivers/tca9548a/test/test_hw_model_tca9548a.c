// SPDX-License-Identifier: MIT
/*
 * test_hw_model_tca9548a.c — tests using TCA9548A bus model
 *
 * Validates TCA9548A driver against a model of the single control byte.
 * Tests channel selection, multi-channel bitmask, disable-all,
 * cache optimization (skip redundant writes), read-back, and the exact
 * bytes each call puts on the bus.
 */

#include "test_harness.h"
#include "hw_model_tca9548a.h"
#include "../tca9548a.h"

#define ADDR 0x70

static tca9548a_model_t model;

static const tca9548a_i2c_t model_i2c = {
    .delay_ms  = tca9548a_model_delay,
    .ctx       = &model,
    .write_raw = tca9548a_model_write_raw,
    .read_raw  = tca9548a_model_read_raw,
};

static void setup(void)
{
    tca9548a_model_init(&model, ADDR);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_disables_all_channels)
{
    setup();
    model.ctrl = 0x5A;   /* left on by an earlier run */
    tca9548a_t dev;
    tca9548a_err_t err = tca9548a_init(&dev, &model_i2c, ADDR);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.ctrl, 0x00);
}

TEST(test_init_rejects_invalid_addr)
{
    setup();
    tca9548a_t dev;
    ASSERT_EQ(tca9548a_init(&dev, &model_i2c, 0x6F), TCA9548A_ERR_PARAM);
    ASSERT_EQ(tca9548a_init(&dev, &model_i2c, 0x78), TCA9548A_ERR_PARAM);
}

TEST(test_init_at_wrong_addr_gets_nack)
{
    setup();
    model.ctrl = 0x5A;
    tca9548a_t dev;
    ASSERT_EQ(tca9548a_init(&dev, &model_i2c, 0x71), TCA9548A_ERR_I2C);
    ASSERT_EQ(model.ctrl, 0x5A);
}

/* --- single channel select --------------------------------------------- */

TEST(test_select_channel_0)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);

    tca9548a_err_t err = tca9548a_select(&dev, 0);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.ctrl, 0x01);
}

TEST(test_select_channel_7)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);

    tca9548a_err_t err = tca9548a_select(&dev, 7);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.ctrl, 0x80);
}

TEST(test_select_channel_out_of_range)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);
    ASSERT_EQ(tca9548a_select(&dev, 8), TCA9548A_ERR_PARAM);
}

/* --- multi-channel ----------------------------------------------------- */

TEST(test_select_multi)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);

    tca9548a_err_t err = tca9548a_select_multi(&dev, 0x05); /* ch0 + ch2 */
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.ctrl, 0x05);
}

/* --- disable all ------------------------------------------------------- */

TEST(test_disable_all)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);

    tca9548a_select(&dev, 3);
    ASSERT_EQ(model.ctrl, 0x08);

    tca9548a_err_t err = tca9548a_disable_all(&dev);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(model.ctrl, 0x00);
}

/* --- cache optimization ------------------------------------------------ */

TEST(test_cache_skips_redundant_write)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);

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
    tca9548a_init(&dev, &model_i2c, ADDR);

    tca9548a_select(&dev, 2);
    int writes_after_first = model.write_count;

    tca9548a_select(&dev, 5);
    ASSERT_TRUE(model.write_count > writes_after_first);
    ASSERT_EQ(model.ctrl, 0x20);
}

/* --- exact bus traffic: Figure 7-4, one byte after the address --------- */

TEST(test_init_sends_one_byte)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);
    ASSERT_EQ(model.write_count, 1);
    ASSERT_EQ(model.last_write_len, 1);
    ASSERT_EQ(model.last_write[0], 0x00);
}

TEST(test_select_sends_one_byte)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);

    ASSERT_EQ(tca9548a_select(&dev, 4), TCA9548A_OK);
    ASSERT_EQ(model.last_write_len, 1);
    ASSERT_EQ(model.last_write[0], 0x10);
}

TEST(test_select_multi_sends_one_byte)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);

    ASSERT_EQ(tca9548a_select_multi(&dev, 0x81), TCA9548A_OK);
    ASSERT_EQ(model.last_write_len, 1);
    ASSERT_EQ(model.last_write[0], 0x81);
}

TEST(test_disable_all_sends_one_byte)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);
    tca9548a_select(&dev, 1);

    ASSERT_EQ(tca9548a_disable_all(&dev), TCA9548A_OK);
    ASSERT_EQ(model.last_write_len, 1);
    ASSERT_EQ(model.last_write[0], 0x00);
}

/* --- read-back: Figure 7-5, the address and then the control byte ------ */

TEST(test_get_channel_reads_control_byte)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);

    tca9548a_select_multi(&dev, 0xA5);

    uint8_t mask = 0;
    tca9548a_err_t err = tca9548a_get_channel(&dev, &mask);
    ASSERT_EQ(err, TCA9548A_OK);
    ASSERT_EQ(mask, 0xA5);
}

TEST(test_get_channel_leaves_the_channels_on)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);
    tca9548a_select_multi(&dev, 0xA5);
    int writes_before = model.write_count;

    uint8_t mask = 0;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_OK);
    ASSERT_EQ(model.write_count, writes_before);
    ASSERT_EQ(model.read_count, 1);
    ASSERT_EQ(model.ctrl, 0xA5);
    ASSERT_EQ(dev.current_channel, 0xA5);
}

TEST(test_get_channel_sees_what_the_chip_holds)
{
    setup();
    tca9548a_t dev;
    tca9548a_init(&dev, &model_i2c, ADDR);
    model.ctrl = 0x03;   /* another controller on the bus changed it */

    uint8_t mask = 0;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_OK);
    ASSERT_EQ(mask, 0x03);
    ASSERT_EQ(dev.current_channel, 0x03);
}

/* --- the chip: every byte after the address replaces the control byte -- */

TEST(test_model_keeps_the_last_byte)
{
    setup();
    const uint8_t two[2] = { 0x11, 0x22 };
    ASSERT_TRUE(tca9548a_model_write_raw(&model, ADDR, two, 2));
    ASSERT_EQ(model.ctrl, 0x22);
}

TEST(test_model_stores_a_register_byte)
{
    setup();
    model.ctrl = 0xA5;
    const uint8_t reg = 0x00;
    ASSERT_TRUE(tca9548a_model_write_raw(&model, ADDR, &reg, 1));
    ASSERT_EQ(model.ctrl, 0x00);
}

/* p.18: "No channel selected, power-up/reset default state" */
TEST(test_model_powers_up_with_no_channel)
{
    setup();
    ASSERT_EQ(model.ctrl, 0x00);
    ASSERT_EQ(model.active, 0x00);
}

/* p.17: "When a channel is selected, the channel becomes active after a stop
 * condition has been placed on the I2C bus." */
TEST(test_model_channel_turns_on_at_the_stop)
{
    setup();
    const uint8_t mask = 0x04;

    model.no_stop = true;
    ASSERT_TRUE(tca9548a_model_write_raw(&model, ADDR, &mask, 1));
    ASSERT_EQ(model.ctrl, 0x04);
    ASSERT_EQ(model.active, 0x00);

    /* The next STOP on the bus does it, here at the end of a read */
    uint8_t val = 0;
    ASSERT_TRUE(tca9548a_model_read_raw(&model, ADDR, &val, 1));
    ASSERT_EQ(val, 0x04);
    ASSERT_EQ(model.active, 0x04);
}

TEST(test_model_channel_turns_off_at_the_stop_too)
{
    setup();
    const uint8_t on = 0x04;
    const uint8_t off = 0x00;

    ASSERT_TRUE(tca9548a_model_write_raw(&model, ADDR, &on, 1));
    model.no_stop = true;
    ASSERT_TRUE(tca9548a_model_write_raw(&model, ADDR, &off, 1));
    ASSERT_EQ(model.active, 0x04);
}

/* p.17: "If multiple bytes are received by the TCA9548A, it saves the last byte
 * received", so only that one reaches the channels */
TEST(test_model_only_the_last_byte_reaches_the_channels)
{
    setup();
    const uint8_t two[2] = { 0x11, 0x22 };
    ASSERT_TRUE(tca9548a_model_write_raw(&model, ADDR, two, 2));
    ASSERT_EQ(model.active, 0x22);
}

/* A NACK at the address changes nothing */
TEST(test_model_wrong_address_reaches_no_channel)
{
    setup();
    const uint8_t mask = 0x04;
    ASSERT_FALSE(tca9548a_model_write_raw(&model, ADDR + 1, &mask, 1));
    ASSERT_EQ(model.active, 0x00);
}

/* p.18: RESET low: "the TCA9548A resets its registers and I2C state machine and
 * deselects all channels" */
TEST(test_model_reset_pin_deselects_all)
{
    setup();
    const uint8_t mask = 0x81;
    ASSERT_TRUE(tca9548a_model_write_raw(&model, ADDR, &mask, 1));

    tca9548a_model_reset_pin(&model);
    ASSERT_EQ(model.ctrl, 0x00);
    ASSERT_EQ(model.active, 0x00);
}

/* The driver's writes end in a STOP, so its channels are really connected */
TEST(test_driver_connects_the_channels)
{
    setup();
    tca9548a_t dev;
    ASSERT_EQ(tca9548a_init(&dev, &model_i2c, ADDR), TCA9548A_OK);
    ASSERT_EQ(model.active, 0x00);

    ASSERT_EQ(tca9548a_select(&dev, 2), TCA9548A_OK);
    ASSERT_EQ(model.active, 0x04);
    ASSERT_EQ(tca9548a_select_multi(&dev, 0x81), TCA9548A_OK);
    ASSERT_EQ(model.active, 0x81);
    ASSERT_EQ(tca9548a_disable_all(&dev), TCA9548A_OK);
    ASSERT_EQ(model.active, 0x00);
}

/* After a RESET pulse the driver's cache is out of date until it reads the chip.
 * tca9548a_get_channel brings it back, and the next select goes out on the bus. */
TEST(test_get_channel_recovers_from_a_reset_pulse)
{
    setup();
    tca9548a_t dev;
    ASSERT_EQ(tca9548a_init(&dev, &model_i2c, ADDR), TCA9548A_OK);
    ASSERT_EQ(tca9548a_select(&dev, 3), TCA9548A_OK);

    tca9548a_model_reset_pin(&model);

    uint8_t mask = 0xFF;
    ASSERT_EQ(tca9548a_get_channel(&dev, &mask), TCA9548A_OK);
    ASSERT_EQ(mask, 0x00);
    ASSERT_EQ(tca9548a_select(&dev, 3), TCA9548A_OK);
    ASSERT_EQ(model.active, 0x08);
}

TEST_MAIN("tca9548a hardware model")
    RUN(test_init_disables_all_channels);
    RUN(test_init_rejects_invalid_addr);
    RUN(test_init_at_wrong_addr_gets_nack);
    RUN(test_select_channel_0);
    RUN(test_select_channel_7);
    RUN(test_select_channel_out_of_range);
    RUN(test_select_multi);
    RUN(test_disable_all);
    RUN(test_cache_skips_redundant_write);
    RUN(test_cache_writes_on_change);
    RUN(test_init_sends_one_byte);
    RUN(test_select_sends_one_byte);
    RUN(test_select_multi_sends_one_byte);
    RUN(test_disable_all_sends_one_byte);
    RUN(test_get_channel_reads_control_byte);
    RUN(test_get_channel_leaves_the_channels_on);
    RUN(test_get_channel_sees_what_the_chip_holds);
    RUN(test_model_keeps_the_last_byte);
    RUN(test_model_stores_a_register_byte);
    RUN(test_model_powers_up_with_no_channel);
    RUN(test_model_channel_turns_on_at_the_stop);
    RUN(test_model_channel_turns_off_at_the_stop_too);
    RUN(test_model_only_the_last_byte_reaches_the_channels);
    RUN(test_model_wrong_address_reaches_no_channel);
    RUN(test_model_reset_pin_deselects_all);
    RUN(test_driver_connects_the_channels);
    RUN(test_get_channel_recovers_from_a_reset_pulse);
TEST_MAIN_END()
