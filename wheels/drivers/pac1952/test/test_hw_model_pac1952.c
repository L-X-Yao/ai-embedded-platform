// SPDX-License-Identifier: MIT
/*
 * test_hw_model_pac1952.c -- tests using the PAC1952 bus model
 *
 * Validates the driver's bus traffic and timing against a model built
 * from DS20006539E, not from the driver. Page numbers are PDF pages.
 */

#include "test_harness.h"
#include "hw_model_pac1952.h"
#include "../pac1952.h"

static pac1952_model_t model;

static const pac1952_i2c_t model_i2c = {
    .read_regs = pac1952_model_read_regs,
    .ctx       = &model,
    .write_raw = pac1952_model_write_raw,
    .delay_ms  = pac1952_model_delay_ms,
};

static const pac1952_cfg_t default_cfg = {
    .sense_resistor_mohm = { 10.0f, 10.0f }
};

static pac1952_t dev;

static void setup(void)
{
    pac1952_model_init(&model);
    (void)pac1952_init(&dev, &model_i2c, &default_cfg);
}

/* --- init --------------------------------------------------------------- */

TEST(test_init_only_reads)
{
    pac1952_model_init(&model);
    ASSERT_EQ(pac1952_init(&dev, &model_i2c, &default_cfg), PAC1952_OK);
    ASSERT_EQ(model.write_count, 0);
}

/* --- REFRESH: Send Byte at address 00h (p40, p44, p47) ------------------ */

TEST(test_refresh_is_one_send_byte)
{
    setup();
    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_OK);
    ASSERT_EQ(model.write_count, 1);
    ASSERT_EQ(model.writes[0].len, 1);
    ASSERT_EQ_HEX(model.writes[0].data[0], 0x00);
    ASSERT_EQ(model.refresh_count, 1);
    ASSERT_EQ(model.refresh_v_count, 0);
}

TEST(test_refresh_waits_out_the_window)
{
    /* p26: stable within 1 ms, and commands inside that 1 ms are NACKed */
    setup();
    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_OK);
    ASSERT_TRUE(model.delay_total_ms >= 1);
    ASSERT_EQ(model.busy_ms, 0);
}

TEST(test_read_straight_after_refresh)
{
    /* The order the header documents: refresh, then read */
    setup();
    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_OK);

    pac1952_channel_t ch;
    ASSERT_EQ(pac1952_read_channel(&dev, 0, &ch), PAC1952_OK);
    ASSERT_NEAR(ch.vbus_v, 12.0f, 0.01f);
    ASSERT_NEAR(ch.vsense_mv, 50.0f, 0.01f);
}

TEST(test_refresh_twice_in_a_row)
{
    /* The second REFRESH lands after the window of the first */
    setup();
    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_OK);
    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_OK);
    ASSERT_EQ(model.refresh_count, 2);
}

TEST(test_readings_stay_until_the_next_refresh)
{
    /* p26: "the values are static until the next REFRESH command" */
    pac1952_channel_t ch;
    setup();
    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_OK);
    pac1952_model_set_vbus_v(&model, 0, 24.0f);

    ASSERT_EQ(pac1952_read_channel(&dev, 0, &ch), PAC1952_OK);
    ASSERT_NEAR(ch.vbus_v, 12.0f, 0.01f);

    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_OK);
    ASSERT_EQ(pac1952_read_channel(&dev, 0, &ch), PAC1952_OK);
    ASSERT_NEAR(ch.vbus_v, 24.0f, 0.01f);
}

TEST(test_refresh_without_write_raw_sends_nothing)
{
    pac1952_i2c_t hal = model_i2c;
    hal.write_raw = NULL;
    pac1952_model_init(&model);
    ASSERT_EQ(pac1952_init(&dev, &hal, &default_cfg), PAC1952_OK);

    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_ERR_PARAM);
    ASSERT_EQ(model.write_count, 0);
    ASSERT_EQ(model.delay_total_ms, 0);
}

TEST(test_refresh_without_delay_sends_nothing)
{
    /* A REFRESH with no wait would leave the next command NACKed */
    pac1952_i2c_t hal = model_i2c;
    hal.delay_ms = NULL;
    pac1952_model_init(&model);
    ASSERT_EQ(pac1952_init(&dev, &hal, &default_cfg), PAC1952_OK);

    ASSERT_EQ(pac1952_refresh(&dev), PAC1952_ERR_PARAM);
    ASSERT_EQ(model.write_count, 0);
}

/* --- CTRL: Block Write, then REFRESH (p26, p41, p48) -------------------- */

TEST(test_set_sample_rate_codes)
{
    /* Register 7-2, p48: 0b0111 = 8 SPS down to 0b0100 = 1024 SPS, in
     * bits [15:12], on the POR value 0730h */
    static const struct {
        pac1952_rate_t rate;
        uint16_t       ctrl;
    } cases[] = {
        { PAC1952_RATE_8SPS,    0x7730 },
        { PAC1952_RATE_64SPS,   0x6730 },
        { PAC1952_RATE_256SPS,  0x5730 },
        { PAC1952_RATE_1024SPS, 0x4730 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        setup();
        ASSERT_EQ(pac1952_set_sample_rate(&dev, cases[i].rate), PAC1952_OK);
        ASSERT_EQ_HEX(model.ctrl, cases[i].ctrl);
        ASSERT_EQ_HEX(model.ctrl_act, cases[i].ctrl);
    }
}

TEST(test_set_sample_rate_bus_traffic)
{
    /* Table 6-9: the register, then both data bytes, in one write.
     * p26: "Changes to the Control register (01h) are activated by
     * sending any REFRESH command". */
    static const uint8_t block[] = { 0x01, 0x77, 0x30 };
    setup();

    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_8SPS), PAC1952_OK);
    ASSERT_EQ(model.write_count, 2);
    ASSERT_EQ(model.writes[0].len, 3);
    ASSERT_MEMEQ_N(model.writes[0].data, block, sizeof(block));
    ASSERT_EQ(model.writes[1].len, 1);
    ASSERT_EQ_HEX(model.writes[1].data[0], 0x00);
    ASSERT_EQ(model.refresh_count, 1);
}

TEST(test_set_sample_rate_keeps_the_other_ctrl_bits)
{
    /* GPIO_ALERT2, SLOW_ALERT1 and CHANNEL_N_OFF are not the rate */
    setup();
    model.ctrl = 0x1A50;

    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_64SPS), PAC1952_OK);
    ASSERT_EQ_HEX(model.ctrl, 0x6A50);
}

TEST(test_set_sample_rate_waits_out_the_window)
{
    pac1952_channel_t ch;
    setup();
    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_8SPS), PAC1952_OK);
    ASSERT_TRUE(model.delay_total_ms >= 1);
    ASSERT_EQ(model.busy_ms, 0);
    ASSERT_EQ(pac1952_read_channel(&dev, 0, &ch), PAC1952_OK);
}

TEST(test_set_sample_rate_without_write_raw_sends_nothing)
{
    pac1952_i2c_t hal = model_i2c;
    hal.write_raw = NULL;
    pac1952_model_init(&model);
    ASSERT_EQ(pac1952_init(&dev, &hal, &default_cfg), PAC1952_OK);
    int ops_before = model.hw.op_count;

    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_8SPS), PAC1952_ERR_PARAM);
    ASSERT_EQ(model.hw.op_count, ops_before);
}

TEST(test_set_sample_rate_without_delay_sends_nothing)
{
    pac1952_i2c_t hal = model_i2c;
    hal.delay_ms = NULL;
    pac1952_model_init(&model);
    ASSERT_EQ(pac1952_init(&dev, &hal, &default_cfg), PAC1952_OK);
    int ops_before = model.hw.op_count;

    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_8SPS), PAC1952_ERR_PARAM);
    ASSERT_EQ(model.hw.op_count, ops_before);
    ASSERT_EQ_HEX(model.ctrl, 0x0730);
}

TEST(test_set_sample_rate_read_fails)
{
    setup();
    model.hw.fail_next = 1;
    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_8SPS), PAC1952_ERR_I2C);
    ASSERT_EQ(model.write_count, 0);
}

TEST(test_set_sample_rate_block_write_fails)
{
    setup();
    model.hw.fail_at = model.hw.op_count + 1;
    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_8SPS), PAC1952_ERR_I2C);
    ASSERT_EQ(model.refresh_count, 0);
    ASSERT_EQ_HEX(model.ctrl, 0x0730);
}

TEST(test_set_sample_rate_refresh_fails)
{
    /* The new value is written but not active, as p47 and p58 allow */
    setup();
    model.hw.fail_at = model.hw.op_count + 2;
    ASSERT_EQ(pac1952_set_sample_rate(&dev, PAC1952_RATE_8SPS), PAC1952_ERR_I2C);
    ASSERT_EQ_HEX(model.ctrl, 0x7730);
    ASSERT_EQ_HEX(model.ctrl_act, 0x0730);
    ASSERT_EQ(model.delay_total_ms, 0);
}

/* --- the chip ----------------------------------------------------------- */

TEST(test_model_refuses_data_after_a_command)
{
    /* p47, p56: "No Data in this command, Send Byte only" */
    static const uint8_t refresh[]   = { 0x00, 0x00 };
    static const uint8_t refresh_v[] = { 0x1F, 0x00 };
    pac1952_model_init(&model);

    ASSERT_FALSE(pac1952_model_write_raw(&model, 0x10, refresh, 2));
    ASSERT_FALSE(pac1952_model_write_raw(&model, 0x10, refresh_v, 2));
    ASSERT_EQ(model.refresh_count, 0);
    ASSERT_EQ(model.refresh_v_count, 0);
}

TEST(test_model_nacks_inside_the_window)
{
    /* p26: "Any new commands written within this 1 ms window will be
     * ignored and NACKed" */
    static const uint8_t refresh[] = { 0x00 };
    uint8_t rx[2];
    pac1952_model_init(&model);

    ASSERT_TRUE(pac1952_model_write_raw(&model, 0x10, refresh, 1));
    ASSERT_FALSE(pac1952_model_write_raw(&model, 0x10, refresh, 1));
    ASSERT_FALSE(pac1952_model_read_regs(&model, 0x10, 0x07, rx, 2));
    ASSERT_EQ(model.refresh_count, 1);

    pac1952_model_delay_ms(&model, 1);
    ASSERT_TRUE(pac1952_model_read_regs(&model, 0x10, 0x07, rx, 2));
}

TEST(test_model_ctrl_reads_back_before_it_is_active)
{
    /* p47: new data "are read back even if no REFRESH command is sent",
     * and p58: CTRL_ACT holds the active value */
    static const uint8_t ctrl[] = { 0x01, 0x47, 0x30 };
    uint8_t rx[2];
    pac1952_model_init(&model);

    ASSERT_TRUE(pac1952_model_write_raw(&model, 0x10, ctrl, 3));
    ASSERT_TRUE(pac1952_model_read_regs(&model, 0x10, 0x01, rx, 2));
    ASSERT_EQ_HEX(rx[0], 0x47);
    ASSERT_TRUE(pac1952_model_read_regs(&model, 0x10, 0x21, rx, 2));
    ASSERT_EQ_HEX(rx[0], 0x07);
}

TEST(test_model_refuses_half_a_ctrl_write)
{
    static const uint8_t half[] = { 0x01, 0x47 };
    pac1952_model_init(&model);

    ASSERT_FALSE(pac1952_model_write_raw(&model, 0x10, half, 2));
    ASSERT_EQ_HEX(model.ctrl, 0x0730);
}

TEST(test_model_refresh_v_acts_like_refresh)
{
    /* p27: REFRESH_V also latches the results and activates CTRL */
    static const uint8_t ctrl[] = { 0x01, 0x47, 0x30 };
    static const uint8_t refresh_v[] = { 0x1F };
    pac1952_model_init(&model);

    ASSERT_TRUE(pac1952_model_write_raw(&model, 0x10, ctrl, 3));
    ASSERT_EQ_HEX(model.ctrl_act, 0x0730);
    ASSERT_TRUE(pac1952_model_write_raw(&model, 0x10, refresh_v, 1));
    ASSERT_EQ_HEX(model.ctrl_act, 0x4730);
    ASSERT_EQ(model.refresh_v_count, 1);
}

TEST_MAIN("pac1952 hardware model")
    RUN(test_init_only_reads);
    RUN(test_refresh_is_one_send_byte);
    RUN(test_refresh_waits_out_the_window);
    RUN(test_read_straight_after_refresh);
    RUN(test_refresh_twice_in_a_row);
    RUN(test_readings_stay_until_the_next_refresh);
    RUN(test_refresh_without_write_raw_sends_nothing);
    RUN(test_refresh_without_delay_sends_nothing);
    RUN(test_set_sample_rate_codes);
    RUN(test_set_sample_rate_bus_traffic);
    RUN(test_set_sample_rate_keeps_the_other_ctrl_bits);
    RUN(test_set_sample_rate_waits_out_the_window);
    RUN(test_set_sample_rate_without_write_raw_sends_nothing);
    RUN(test_set_sample_rate_without_delay_sends_nothing);
    RUN(test_set_sample_rate_read_fails);
    RUN(test_set_sample_rate_block_write_fails);
    RUN(test_set_sample_rate_refresh_fails);
    RUN(test_model_refuses_data_after_a_command);
    RUN(test_model_nacks_inside_the_window);
    RUN(test_model_ctrl_reads_back_before_it_is_active);
    RUN(test_model_refuses_half_a_ctrl_write);
    RUN(test_model_refresh_v_acts_like_refresh);
TEST_MAIN_END()
