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

    ASSERT_EQ(drv2605_set_waveform(&dev, 0, 1), DRV2605_OK);
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

    ASSERT_EQ(drv2605_set_waveform(&dev, 0, 1), DRV2605_OK);
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

/* --- the model, from SLOS854D ------------------------------------------ */

/* Table 3, p.35, every register */
TEST(test_reset_values_follow_table_3)
{
    static const uint8_t table3[0x23] = {
        0xE0, 0x40, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x05, 0x19, 0xFF, 0x19, 0xFF, 0x3E, 0x8C,
        0x0C, 0x6C, 0x36, 0x93, 0xF5, 0xA0, 0x20, 0x80,
        0x33, 0x00, 0x00,
    };
    setup();
    for (unsigned reg = 0; reg < sizeof(table3); reg++) {
        ASSERT_EQ_HEX(model.hw.regs[reg], table3[reg]);
    }
}

/* Table 3: MODE bits 5:3 and the library bits 7:5 and 3 are reserved */
TEST(test_reserved_bits_stay_clear)
{
    setup();
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x3F);
    ASSERT_EQ_HEX(model.hw.regs[DRV2605_REG_MODE], 0x07);
    hw_model__write_reg(&model.hw, 0x03, 0xFF);
    ASSERT_EQ_HEX(model.hw.regs[0x03], 0x17);
}

/* A ready chip in internal trigger mode with one effect queued */
static void ready_with_an_effect(void)
{
    setup();
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x00);
    hw_model__write_reg(&model.hw, DRV2605_REG_WAVEFORM0, 0x01);
}

/* p.22: "The GO bit remains high until the playback of the haptic waveform
 * sequence is complete." */
TEST(test_go_clears_itself_when_playback_ends)
{
    ready_with_an_effect();
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_GO], 0x01);
    ASSERT_TRUE(model.playing);

    drv2605_model_end_playback(&model);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_GO], 0x00);
    ASSERT_FALSE(model.playing);
    ASSERT_EQ(model.stop_count, 0);
}

/* p.22: "Clearing the GO bit during waveform playback cancels the waveform
 * sequence." */
TEST(test_clearing_go_cancels)
{
    ready_with_an_effect();
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x00);
    ASSERT_FALSE(model.playing);
    ASSERT_EQ(model.stop_count, 1);
}

TEST(test_go_can_fire_again_after_playback)
{
    ready_with_an_effect();
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    drv2605_model_end_playback(&model);
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    ASSERT_EQ(model.go_count, 2);
    ASSERT_TRUE(model.playing);
}

/* p.39: the sequencer runs "until the sequencer reaches an identifier value of
 * zero", and p.16 starts identifiers at 1, so a zero at 0x04 queues nothing */
TEST(test_go_on_an_empty_sequencer_ends_at_once)
{
    setup();
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x00);
    hw_model__write_reg(&model.hw, DRV2605_REG_WAVEFORM0, 0x00);
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    ASSERT_FALSE(model.playing);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_GO], 0x00);
}

/* p.37: "0: Internal trigger / Waveforms are fired by setting the GO bit" */
TEST(test_go_plays_only_in_internal_trigger_mode)
{
    ready_with_an_effect();
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x05);
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    ASSERT_FALSE(model.playing);
}

/* p.21: "the DRV2605L device goes into a low-power state" */
TEST(test_go_plays_nothing_in_standby)
{
    ready_with_an_effect();
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x40);
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    ASSERT_FALSE(model.playing);
}

/* p.21: "if a waveform is played, it immediately stops when the STANDBY bit is
 * asserted", and "the device retains register values" */
TEST(test_standby_stops_playback_and_keeps_registers)
{
    ready_with_an_effect();
    hw_model__write_reg(&model.hw, DRV2605_REG_RTP, 0x55);
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x40);
    ASSERT_FALSE(model.playing);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_GO], 0x00);
    ASSERT_EQ(model.hw.regs[DRV2605_REG_RTP], 0x55);
}

/* p.37: "Any playback operations are immediately interrupted, and all registers
 * are reset to the default values. The DEV_RESET bit self-clears" */
TEST(test_dev_reset_is_a_power_cycle)
{
    ready_with_an_effect();
    hw_model__write_reg(&model.hw, DRV2605_REG_RTP, 0x55);
    hw_model__write_reg(&model.hw, DRV2605_REG_GO, 0x01);
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x80);
    ASSERT_FALSE(model.playing);
    ASSERT_EQ_HEX(model.hw.regs[DRV2605_REG_MODE], 0x40);
    ASSERT_EQ_HEX(model.hw.regs[DRV2605_REG_RTP], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[DRV2605_REG_GO], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[0x16], 0x3E);
}

/* p.38: the engine drives RTP_INPUT "when MODE[2:0] = 5 (RTP mode)" */
TEST(test_rtp_drives_only_in_rtp_mode)
{
    setup();
    ASSERT_FALSE(drv2605_model_rtp_driving(&model));
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x45);
    ASSERT_FALSE(drv2605_model_rtp_driving(&model));  /* still in standby */
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x05);
    ASSERT_TRUE(drv2605_model_rtp_driving(&model));
    hw_model__write_reg(&model.hw, DRV2605_REG_MODE, 0x00);
    ASSERT_FALSE(drv2605_model_rtp_driving(&model));
}

/* --- the driver on it --------------------------------------------------- */

TEST(test_driver_plays_again_after_the_chip_finished)
{
    setup();
    drv2605_t dev;
    ASSERT_EQ(drv2605_init(&dev, &model_i2c), DRV2605_OK);
    ASSERT_EQ(drv2605_set_waveform(&dev, 0, 47), DRV2605_OK);
    ASSERT_EQ(drv2605_go(&dev), DRV2605_OK);
    ASSERT_TRUE(model.playing);
    drv2605_model_end_playback(&model);
    ASSERT_EQ(drv2605_go(&dev), DRV2605_OK);
    ASSERT_TRUE(model.playing);
    ASSERT_EQ(model.go_count, 2);
}

/* A stop after the end has nothing to cancel, and must not fail */
TEST(test_driver_stop_after_the_chip_finished)
{
    setup();
    drv2605_t dev;
    ASSERT_EQ(drv2605_init(&dev, &model_i2c), DRV2605_OK);
    ASSERT_EQ(drv2605_set_waveform(&dev, 0, 47), DRV2605_OK);
    ASSERT_EQ(drv2605_go(&dev), DRV2605_OK);
    drv2605_model_end_playback(&model);
    ASSERT_EQ(drv2605_stop(&dev), DRV2605_OK);
    ASSERT_EQ(model.stop_count, 0);
}

/* p.33, p.38: RTP drives the motor until the mode changes */
TEST(test_driver_rtp_runs_until_stop)
{
    setup();
    drv2605_t dev;
    ASSERT_EQ(drv2605_init(&dev, &model_i2c), DRV2605_OK);
    ASSERT_EQ(drv2605_set_realtime_value(&dev, 0x40), DRV2605_OK);
    ASSERT_TRUE(drv2605_model_rtp_driving(&model));
    ASSERT_EQ(drv2605_stop(&dev), DRV2605_OK);
    ASSERT_FALSE(drv2605_model_rtp_driving(&model));
}

/* drv2605_go after RTP has to leave RTP mode, or GO plays nothing */
TEST(test_driver_go_after_rtp_plays)
{
    setup();
    drv2605_t dev;
    ASSERT_EQ(drv2605_init(&dev, &model_i2c), DRV2605_OK);
    ASSERT_EQ(drv2605_set_waveform(&dev, 0, 47), DRV2605_OK);
    ASSERT_EQ(drv2605_set_realtime_value(&dev, 0x40), DRV2605_OK);
    ASSERT_EQ(drv2605_go(&dev), DRV2605_OK);
    ASSERT_TRUE(model.playing);
    ASSERT_FALSE(drv2605_model_rtp_driving(&model));
}

/* p.59 step 3: "Write the MODE register (address 0x01) to value 0x00 to remove
 * the device from standby mode", so a play right after init works */
TEST(test_driver_plays_straight_after_init)
{
    setup();
    drv2605_t dev;
    ASSERT_EQ(drv2605_init(&dev, &model_i2c), DRV2605_OK);
    ASSERT_EQ(drv2605_set_waveform(&dev, 0, 1), DRV2605_OK);
    ASSERT_EQ(drv2605_go(&dev), DRV2605_OK);
    ASSERT_TRUE(model.playing);
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
    RUN(test_reset_values_follow_table_3);
    RUN(test_reserved_bits_stay_clear);
    RUN(test_go_clears_itself_when_playback_ends);
    RUN(test_clearing_go_cancels);
    RUN(test_go_can_fire_again_after_playback);
    RUN(test_go_on_an_empty_sequencer_ends_at_once);
    RUN(test_go_plays_only_in_internal_trigger_mode);
    RUN(test_go_plays_nothing_in_standby);
    RUN(test_standby_stops_playback_and_keeps_registers);
    RUN(test_dev_reset_is_a_power_cycle);
    RUN(test_rtp_drives_only_in_rtp_mode);
    RUN(test_driver_plays_again_after_the_chip_finished);
    RUN(test_driver_stop_after_the_chip_finished);
    RUN(test_driver_rtp_runs_until_stop);
    RUN(test_driver_go_after_rtp_plays);
    RUN(test_driver_plays_straight_after_init);
TEST_MAIN_END()
