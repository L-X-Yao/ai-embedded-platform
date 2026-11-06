// SPDX-License-Identifier: MIT
/*
 * test_hw_model_tcs34725.c — tests using hardware register model
 *
 * Validates TCS34725 driver against a datasheet-accurate register model.
 * The model enforces read-only chip ID (0x44), RGBC data register
 * protection, the command byte's TYPE field, enable/disable behavior.
 */

#include "test_harness.h"
#include "hw_model_tcs34725.h"
#include "../tcs34725.h"
#include <math.h>

static tcs34725_model_t model;

static const tcs34725_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = tcs34725_model_read_regs,
    .delay_ms  = tcs34725_model_delay_ms,
    .ctx       = &model.hw,
};

static void setup(void)
{
    tcs34725_model_init(&model);
}

/* PON, the 2.4 ms of p.15, then AEN, with ATIME as given */
static void start_rgbc(uint8_t atime)
{
    hw_model__write_reg(&model.hw, TCS_REG_ATIME, atime);
    hw_model__write_reg(&model.hw, TCS_REG_ENABLE, 0x01);
    tcs34725_model_delay_ms(&model.hw, 3);
    hw_model__write_reg(&model.hw, TCS_REG_ENABLE, 0x03);
}

/* p.10: RGBC Init takes 2.4 ms, then p.8: 2.4 ms x (256 - 0xD5) = 103.2 ms */
#define CYCLE_AT_101MS 106

static uint16_t clear_count(void)
{
    return (uint16_t)(model.hw.regs[0x94] | (model.hw.regs[0x95] << 8));
}

static uint8_t avalid(void)
{
    return model.hw.regs[TCS_REG_STATUS] & 0x01u;
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &model_i2c,
                                        TCS34725_INTEG_101MS,
                                        TCS34725_GAIN_1X);
    ASSERT_EQ(err, TCS34725_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[TCS_REG_ID] = 0xFF;
    hw_model_reset(&model.hw);

    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &model_i2c,
                                        TCS34725_INTEG_101MS,
                                        TCS34725_GAIN_1X);
    ASSERT_EQ(err, TCS34725_ERR_ID);
}

TEST(test_init_accepts_variant_id)
{
    setup();
    /* 0x4D is an accepted variant */
    model.hw.reset_values[TCS_REG_ID] = 0x4Du;
    hw_model_reset(&model.hw);

    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &model_i2c,
                                        TCS34725_INTEG_101MS,
                                        TCS34725_GAIN_1X);
    ASSERT_EQ(err, TCS34725_OK);
}

TEST(test_init_sets_integration_time)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    /* ATIME register at 0x81 should be 0xD5 */
    ASSERT_EQ(model.hw.regs[TCS_REG_ATIME], 0xD5u);
}

TEST(test_init_sets_gain)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_4X);
    /* Control register at 0x8F should be 0x01 */
    ASSERT_EQ(model.hw.regs[TCS_REG_CONTROL], 0x01u);
}

TEST(test_init_enables_pon_aen)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    /* Enable register should have PON (0x01) + AEN (0x02) = 0x03 */
    ASSERT_EQ(model.hw.regs[TCS_REG_ENABLE], 0x03u);
}

TEST(test_init_refuses_missing_delay)
{
    /* With no wait AEN cannot follow PON by the 2.4 ms of p.15 */
    setup();
    tcs34725_i2c_t no_delay = model_i2c;
    no_delay.delay_ms = NULL;
    tcs34725_t dev;
    tcs34725_err_t err = tcs34725_init(&dev, &no_delay,
                                        TCS34725_INTEG_101MS,
                                        TCS34725_GAIN_1X);
    ASSERT_EQ(err, TCS34725_ERR_PARAM);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_rgbc)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    tcs34725_model_delay_ms(&model.hw, CYCLE_AT_101MS);

    tcs34725_rgbc_t rgbc;
    tcs34725_err_t err = tcs34725_read_rgbc(&dev, &rgbc);
    ASSERT_EQ(err, TCS34725_OK);
    ASSERT_EQ(rgbc.c, 5000);
    ASSERT_EQ(rgbc.r, 1800);
    ASSERT_EQ(rgbc.g, 2200);
    ASSERT_EQ(rgbc.b, 1000);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_byte_protocol_repeats_register)
{
    setup();
    tcs34725_model_set_color_data(&model);

    uint8_t buf[2];
    ASSERT_TRUE(tcs34725_model_read_regs(&model.hw, TCS34725_ADDR, 0x94u, buf, 2));
    ASSERT_EQ(buf[0], 0x88u);  /* 5000 = 0x1388 */
    ASSERT_EQ(buf[1], 0x88u);
}

TEST(test_block_protocol_steps_through_registers)
{
    setup();
    tcs34725_model_set_color_data(&model);

    uint8_t buf[2];
    ASSERT_TRUE(tcs34725_model_read_regs(&model.hw, TCS34725_ADDR, 0xB4u, buf, 2));
    ASSERT_EQ(buf[0], 0x88u);
    ASSERT_EQ(buf[1], 0x13u);
}

TEST(test_pon_and_aen_together_start_nothing)
{
    setup();
    hw_model__write_reg(&model.hw, TCS_REG_ENABLE, 0x03);
    tcs34725_model_delay_ms(&model.hw, 1000);
    ASSERT_EQ(model.hw.regs[TCS_REG_CDATAL], 0x00u);
    ASSERT_EQ(avalid(), 0);
}

TEST(test_aen_held_from_pon_starts_nothing)
{
    /* AEN has to rise after the interval, not just still be set after it */
    setup();
    hw_model__write_reg(&model.hw, TCS_REG_ENABLE, 0x03);
    tcs34725_model_delay_ms(&model.hw, 3);
    hw_model__write_reg(&model.hw, TCS_REG_ENABLE, 0x03);
    tcs34725_model_delay_ms(&model.hw, 1000);
    ASSERT_EQ(model.hw.regs[TCS_REG_CDATAL], 0x00u);
    ASSERT_EQ(avalid(), 0);
}

TEST(test_aen_after_pon_interval_starts_rgbc)
{
    setup();
    start_rgbc(0xD5);
    tcs34725_model_delay_ms(&model.hw, CYCLE_AT_101MS);
    ASSERT_EQ(model.hw.regs[TCS_REG_CDATAL], 0x88u);  /* 5000 = 0x1388 */
}

TEST(test_enable_waits_after_pon)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    tcs34725_disable(&dev);

    model.true_c = 1234;
    ASSERT_EQ(tcs34725_enable(&dev), TCS34725_OK);
    tcs34725_model_delay_ms(&model.hw, CYCLE_AT_101MS);

    tcs34725_rgbc_t rgbc;
    ASSERT_EQ(tcs34725_read_rgbc(&dev, &rgbc), TCS34725_OK);
    ASSERT_EQ(rgbc.c, 1234);
}

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, TCS_REG_ID, 0x00);
    ASSERT_EQ(model.hw.regs[TCS_REG_ID], 0x44u);
}

TEST(test_rgbc_data_readonly)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    tcs34725_model_delay_ms(&model.hw, CYCLE_AT_101MS);

    uint8_t orig = model.hw.regs[TCS_REG_CDATAL];
    ASSERT_EQ(orig, 0x88u);
    hw_model__write_reg(&model.hw, TCS_REG_CDATAL, 0xFF);
    ASSERT_EQ(model.hw.regs[TCS_REG_CDATAL], orig);
}

TEST(test_disable_clears_enable)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    ASSERT_EQ(model.hw.regs[TCS_REG_ENABLE], 0x03u);

    tcs34725_disable(&dev);
    ASSERT_EQ(model.hw.regs[TCS_REG_ENABLE], 0x00u);
}

TEST(test_change_gain_runtime)
{
    setup();
    tcs34725_t dev;
    tcs34725_init(&dev, &model_i2c, TCS34725_INTEG_101MS, TCS34725_GAIN_1X);
    ASSERT_EQ(model.hw.regs[TCS_REG_CONTROL], 0x00u);

    tcs34725_set_gain(&dev, TCS34725_GAIN_60X);
    ASSERT_EQ(model.hw.regs[TCS_REG_CONTROL], 0x03u);
}

/* --- reset values, Table 3, p.13 --------------------------------------- */

TEST(test_reset_values)
{
    setup();
    ASSERT_EQ_HEX(model.hw.regs[TCS_REG_ENABLE], 0x00u);
    ASSERT_EQ_HEX(model.hw.regs[TCS_REG_ATIME], 0xFFu);
    ASSERT_EQ_HEX(model.hw.regs[TCS_REG_WTIME], 0xFFu);
    ASSERT_EQ_HEX(model.hw.regs[TCS_REG_CONTROL], 0x00u);
    ASSERT_EQ_HEX(model.hw.regs[TCS_REG_STATUS], 0x00u);
    for (unsigned i = 0; i < 8; i++) {
        ASSERT_EQ_HEX(model.hw.regs[TCS_REG_CDATAL + i], 0x00u);
    }
}

/* Table 3: STATUS is type R, and p.19: "This register is read only." */
TEST(test_status_is_read_only)
{
    setup();
    hw_model__write_reg(&model.hw, TCS_REG_STATUS, 0x11);
    ASSERT_EQ_HEX(model.hw.regs[TCS_REG_STATUS], 0x00u);
}

/* --- the integration cycle --------------------------------------------- */

/* p.10: "The RGBC Init state takes 2.4 ms", and p.8: "Integration Time = 2.4 ms
 * x (256 - ATIME)". p.19: AVALID "Indicates that the RGBC channels have
 * completed an integration cycle." 10 cycles make 26.4 ms in all. */
TEST(test_a_cycle_is_init_plus_integration)
{
    setup();
    start_rgbc(0xF6);
    tcs34725_model_delay_ms(&model.hw, 26);
    ASSERT_EQ(avalid(), 0);
    ASSERT_EQ(clear_count(), 0);

    tcs34725_model_delay_ms(&model.hw, 1);
    ASSERT_EQ(avalid(), 1);
    ASSERT_EQ(clear_count(), 5000);
}

/* Table 3: ATIME resets to 0xFF, a single 2.4 ms step. p.16: that step counts
 * up to (256 - 255) x 1024. */
TEST(test_reset_atime_is_the_shortest_cycle)
{
    setup();
    hw_model__write_reg(&model.hw, TCS_REG_ENABLE, 0x01);
    tcs34725_model_delay_ms(&model.hw, 3);
    hw_model__write_reg(&model.hw, TCS_REG_ENABLE, 0x03);
    tcs34725_model_delay_ms(&model.hw, 4);
    ASSERT_EQ(avalid(), 0);
    tcs34725_model_delay_ms(&model.hw, 1);
    ASSERT_EQ(avalid(), 1);
    ASSERT_EQ(clear_count(), 1024);
}

/* p.16: "Max RGBC Count = (256 - ATIME) x 1024 up to a maximum of 65535." */
TEST(test_counts_stop_at_the_maximum)
{
    setup();
    model.true_c = 65535;
    start_rgbc(0xF6);
    tcs34725_model_delay_ms(&model.hw, 27);
    ASSERT_EQ(clear_count(), 10240);

    setup();
    model.true_c = 65535;
    start_rgbc(0x00);
    tcs34725_model_delay_ms(&model.hw, 617);
    ASSERT_EQ(clear_count(), 65535);
}

/* The registers hold the last finished cycle, p.19 */
TEST(test_new_light_waits_for_the_end_of_a_cycle)
{
    setup();
    start_rgbc(0xF6);
    tcs34725_model_delay_ms(&model.hw, 27);
    model.true_c = 700;
    ASSERT_EQ(clear_count(), 5000);
    tcs34725_model_delay_ms(&model.hw, 27);
    ASSERT_EQ(clear_count(), 700);
}

/* p.15: AEN "Writing a 0 disables the RGBC" */
TEST(test_no_cycle_after_aen_falls)
{
    setup();
    start_rgbc(0xF6);
    tcs34725_model_delay_ms(&model.hw, 27);
    hw_model__write_reg(&model.hw, TCS_REG_ENABLE, 0x01);
    model.true_c = 700;
    tcs34725_model_delay_ms(&model.hw, 100);
    ASSERT_EQ(clear_count(), 5000);
    ASSERT_EQ(avalid(), 0);
}

TEST_MAIN("tcs34725 hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_accepts_variant_id);
    RUN(test_init_sets_integration_time);
    RUN(test_init_sets_gain);
    RUN(test_init_enables_pon_aen);
    RUN(test_init_refuses_missing_delay);
    RUN(test_read_rgbc);
    RUN(test_byte_protocol_repeats_register);
    RUN(test_block_protocol_steps_through_registers);
    RUN(test_pon_and_aen_together_start_nothing);
    RUN(test_aen_held_from_pon_starts_nothing);
    RUN(test_aen_after_pon_interval_starts_rgbc);
    RUN(test_enable_waits_after_pon);
    RUN(test_chip_id_readonly);
    RUN(test_rgbc_data_readonly);
    RUN(test_disable_clears_enable);
    RUN(test_change_gain_runtime);
    RUN(test_reset_values);
    RUN(test_status_is_read_only);
    RUN(test_a_cycle_is_init_plus_integration);
    RUN(test_reset_atime_is_the_shortest_cycle);
    RUN(test_counts_stop_at_the_maximum);
    RUN(test_new_light_waits_for_the_end_of_a_cycle);
    RUN(test_no_cycle_after_aen_falls);
TEST_MAIN_END()
