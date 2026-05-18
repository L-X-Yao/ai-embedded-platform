// SPDX-License-Identifier: MIT
/*
 * test_hw_model_drv2605.c — tests using DRV2605L hardware register model
 *
 * Validates DRV2605L driver against a datasheet-accurate register model.
 * Tests device ID verification, mode register, waveform sequencer,
 * GO bit behavior, actuator type selection, and RTP value.
 */

#include "test_harness.h"
#include "hw_model_drv2605.h"
#include "../drv2605.h"

static drv2605_model_t model;

static const drv2605_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = drv2605_model_delay,
    .ctx       = &model.hw,
};

static void setup(void)
{
    drv2605_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_verifies_device_id)
{
    setup();
    drv2605_t dev;
    drv2605_err_t err = drv2605_init(&dev, &model_i2c);
    ASSERT_EQ(err, DRV2605_OK);
}

TEST(test_init_rejects_wrong_device_id)
{
    setup();
    /* Set an invalid device ID */
    drv2605_model_set_devid(&model, 0);
    drv2605_t dev;
    drv2605_err_t err = drv2605_init(&dev, &model_i2c);
    ASSERT_EQ(err, DRV2605_ERR_ID);
}

TEST(test_init_accepts_drv2605_id)
{
    setup();
    /* DRV2605 (non-L) has device ID 3 */
    drv2605_model_set_devid(&model, 3);
    drv2605_t dev;
    drv2605_err_t err = drv2605_init(&dev, &model_i2c);
    ASSERT_EQ(err, DRV2605_OK);
}

TEST(test_init_exits_standby)
{
    setup();
    drv2605_t dev;
    drv2605_init(&dev, &model_i2c);
    /* MODE register should be 0x00 (internal trigger, not standby 0x40) */
    ASSERT_EQ(model.hw.regs[DRV2605_REG_MODE], 0x00);
}

TEST(test_init_clears_waveform_slots)
{
    setup();
    /* Pre-fill waveform slots */
    for (int i = 0; i < 8; i++) {
        model.hw.regs[DRV2605_REG_WAVEFORM0 + i] = 0xFF;
    }
    drv2605_t dev;
    drv2605_init(&dev, &model_i2c);
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(model.hw.regs[DRV2605_REG_WAVEFORM0 + i], 0x00);
    }
}

/* --- status register protection ---------------------------------------- */

TEST(test_status_register_readonly)
{
    setup();
    uint8_t original = model.hw.regs[DRV2605_REG_STATUS];
    hw_model__write_reg(&model.hw, DRV2605_REG_STATUS, 0x00);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_STATUS], original);
}

/* --- waveform sequencer ------------------------------------------------ */

TEST(test_set_waveform_slot)
{
    setup();
    drv2605_t dev;
    drv2605_init(&dev, &model_i2c);

    drv2605_err_t err = drv2605_set_waveform(&dev, 0, 47);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_WAVEFORM0], 47);

    err = drv2605_set_waveform(&dev, 7, 123);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_WAVEFORM0 + 7], 123);
}

/* --- GO bit ------------------------------------------------------------ */

TEST(test_go_triggers_playback)
{
    setup();
    drv2605_t dev;
    drv2605_init(&dev, &model_i2c);

    int before = model.go_count;
    drv2605_err_t err = drv2605_go(&dev);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_EQ(model.go_count, before + 1);
    ASSERT_TRUE(model.playing);
}

TEST(test_stop_clears_go)
{
    setup();
    drv2605_t dev;
    drv2605_init(&dev, &model_i2c);

    drv2605_go(&dev);
    ASSERT_TRUE(model.playing);

    drv2605_err_t err = drv2605_stop(&dev);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_FALSE(model.playing);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_GO] & 0x01, 0x00);
}

TEST(test_go_register_only_bit0_writable)
{
    setup();
    /* Try writing full byte to GO register */
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0xFF);
    /* Only bit 0 should be set */
    ASSERT_EQ(model.hw.regs[DRV2605_REG_GO], 0x01);
}

/* --- actuator type ----------------------------------------------------- */

TEST(test_set_actuator_lra)
{
    setup();
    drv2605_t dev;
    drv2605_init(&dev, &model_i2c);

    drv2605_err_t err = drv2605_set_actuator(&dev, DRV2605_ACTUATOR_LRA);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_TRUE((model.hw.regs[DRV2605_REG_FEEDBACK] & DRV2605_FEEDBACK_LRA_BIT) != 0);
}

TEST(test_set_actuator_erm)
{
    setup();
    drv2605_t dev;
    drv2605_init(&dev, &model_i2c);

    /* First set to LRA, then back to ERM */
    drv2605_set_actuator(&dev, DRV2605_ACTUATOR_LRA);
    drv2605_err_t err = drv2605_set_actuator(&dev, DRV2605_ACTUATOR_ERM);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_TRUE((model.hw.regs[DRV2605_REG_FEEDBACK] & DRV2605_FEEDBACK_LRA_BIT) == 0);
}

/* --- RTP --------------------------------------------------------------- */

TEST(test_set_realtime_value)
{
    setup();
    drv2605_t dev;
    drv2605_init(&dev, &model_i2c);

    drv2605_err_t err = drv2605_set_realtime_value(&dev, 0x7F);
    ASSERT_EQ(err, DRV2605_OK);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_RTP], 0x7F);
}

TEST_MAIN("drv2605 hardware model")
    RUN(test_init_verifies_device_id);
    RUN(test_init_rejects_wrong_device_id);
    RUN(test_init_accepts_drv2605_id);
    RUN(test_init_exits_standby);
    RUN(test_init_clears_waveform_slots);
    RUN(test_status_register_readonly);
    RUN(test_set_waveform_slot);
    RUN(test_go_triggers_playback);
    RUN(test_stop_clears_go);
    RUN(test_go_register_only_bit0_writable);
    RUN(test_set_actuator_lra);
    RUN(test_set_actuator_erm);
    RUN(test_set_realtime_value);
TEST_MAIN_END()
