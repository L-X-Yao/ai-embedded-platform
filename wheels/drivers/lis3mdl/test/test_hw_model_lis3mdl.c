// SPDX-License-Identifier: MIT
/*
 * test_hw_model_lis3mdl.c — tests using hardware register model
 *
 * Validates LIS3MDL driver against a datasheet-accurate register model.
 * The model enforces read-only WHO_AM_I (0x3D), data register protection,
 * config register writable masks, the SUB auto-increment bit, and a sample
 * clock behind STATUS_REG. Time passes only through the delay callback, so
 * tests wait for a sample the way a caller has to.
 */

#include "test_harness.h"
#include "hw_model_lis3mdl.h"
#include "../lis3mdl.h"
#include <math.h>

static lis3mdl_model_t model;

static const lis3mdl_i2c_t model_i2c = {
    .write_reg = lis3mdl_hw_model_write_reg,
    .read_regs = lis3mdl_hw_model_read_regs,
    .delay_ms  = lis3mdl_model_delay_ms,
    .ctx       = &model.hw,
};

static const lis3mdl_cfg_t default_cfg = {
    .range = LIS3MDL_RANGE_4G,
    .addr  = LIS3MDL_ADDR_LOW,
};

static void setup(void)
{
    lis3mdl_model_init(&model);
}

static void wait_ms(uint32_t ms)
{
    lis3mdl_model_delay_ms(&model.hw, ms);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t val = 0;
    model_i2c.read_regs(&model.hw, LIS3MDL_ADDR_LOW, reg, &val, 1);
    return val;
}

static void write_reg(uint8_t reg, uint8_t val)
{
    model_i2c.write_reg(&model.hw, LIS3MDL_ADDR_LOW, reg, val);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_hw_init_por_defaults)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_err_t err = lis3mdl_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, LIS3MDL_OK);
}

TEST(test_hw_init_wrong_id)
{
    setup();
    /* Corrupt WHO_AM_I: 0x3D -> 0x00 */
    model.hw.reset_values[LIS3MDL_REG_WHO_AM_I] = 0x00;
    hw_model_reset(&model.hw);

    lis3mdl_t dev;
    lis3mdl_err_t err = lis3mdl_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, LIS3MDL_ERR_ID);
}

TEST(test_hw_init_sets_ctrl_reg1)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    /* CTRL_REG1 should be 0x30 (XY medium, 10 Hz) */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG1], 0x30u);
}

TEST(test_hw_init_sets_ctrl_reg2_4g)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = { .range = LIS3MDL_RANGE_4G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG2: range=0 -> bits[6:5]=00 -> 0x00 */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x00u);
}

TEST(test_hw_init_sets_ctrl_reg2_8g)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = { .range = LIS3MDL_RANGE_8G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG2: range=1 -> bits[6:5]=01 -> 0x20 */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x20u);
}

TEST(test_hw_init_sets_ctrl_reg2_12g)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = { .range = LIS3MDL_RANGE_12G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG2: range=2 -> bits[6:5]=10 -> 0x40 */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x40u);
}

TEST(test_hw_init_sets_ctrl_reg2_16g)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_cfg_t cfg = { .range = LIS3MDL_RANGE_16G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg);
    /* CTRL_REG2: range=3 -> bits[6:5]=11 -> 0x60 */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x60u);
}

TEST(test_hw_init_sets_continuous_mode)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    /* CTRL_REG3 = 0x00 = continuous */
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG3], 0x00u);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_hw_read_round_trip)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    wait_ms(100);

    lis3mdl_raw_t raw;
    lis3mdl_err_t err = lis3mdl_read_raw(&dev, &raw);
    ASSERT_EQ(err, LIS3MDL_OK);
    ASSERT_EQ(raw.x, model.true_x);
    ASSERT_EQ(raw.y, model.true_y);
    ASSERT_EQ(raw.z, model.true_z);
}

/* p.28: ZYXDA says a sample is there. The first comes 1/ODR after init, 100 ms
 * at 10 Hz, and until then the output registers hold no measurement. */
TEST(test_read_before_the_first_sample_is_not_ready)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);

    lis3mdl_raw_t raw = { 1, 2, 3 };
    lis3mdl_data_t data;
    ASSERT_EQ(lis3mdl_read_raw(&dev, &raw), LIS3MDL_ERR_NOT_READY);
    ASSERT_EQ(raw.x, 1);
    ASSERT_EQ(lis3mdl_read(&dev, &data), LIS3MDL_ERR_NOT_READY);

    wait_ms(99);
    ASSERT_EQ(lis3mdl_read_raw(&dev, &raw), LIS3MDL_ERR_NOT_READY);
    wait_ms(1);
    ASSERT_EQ(lis3mdl_read_raw(&dev, &raw), LIS3MDL_OK);
    ASSERT_EQ(raw.x, model.true_x);
}

/* Once a sample exists a read between samples stays what it was: the last
 * sample and OK, so a caller polling faster than the ODR notices nothing. */
TEST(test_read_between_samples_stays_ok)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    wait_ms(100);

    lis3mdl_raw_t first;
    lis3mdl_raw_t again;
    ASSERT_EQ(lis3mdl_read_raw(&dev, &first), LIS3MDL_OK);
    ASSERT_EQ(lis3mdl_read_raw(&dev, &again), LIS3MDL_OK);
    ASSERT_EQ(first.x, again.x);
    ASSERT_EQ(first.y, again.y);
    ASSERT_EQ(first.z, again.z);
}

/* A real sample of exactly zero is a sample: ZYXDA tells it from no sample. */
TEST(test_sample_of_zero_is_a_sample)
{
    setup();
    model.true_x = 0;
    model.true_y = 0;
    model.true_z = 0;
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    wait_ms(100);

    lis3mdl_raw_t raw = { 1, 2, 3 };
    ASSERT_EQ(lis3mdl_read_raw(&dev, &raw), LIS3MDL_OK);
    ASSERT_EQ(raw.x, 0);
    ASSERT_EQ(raw.y, 0);
    ASSERT_EQ(raw.z, 0);
}

TEST(test_hw_read_ut_4g)
{
    setup();
    /* Inject 50 uT on X at 4g range */
    lis3mdl_model_inject_ut(&model, 50.0f, -30.0f, 0.0f, 6842.0f);

    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    wait_ms(100);

    lis3mdl_data_t data;
    lis3mdl_err_t err = lis3mdl_read(&dev, &data);
    ASSERT_EQ(err, LIS3MDL_OK);
    /* Tolerance: truncation from int16 cast */
    ASSERT_NEAR(data.x_ut, 50.0f, 0.5f);
    ASSERT_NEAR(data.y_ut, -30.0f, 0.5f);
    ASSERT_NEAR(data.z_ut, 0.0f, 0.5f);
}

TEST(test_hw_read_ut_16g)
{
    setup();
    lis3mdl_model_inject_ut(&model, 200.0f, -100.0f, 50.0f, 1711.0f);

    lis3mdl_t dev;
    lis3mdl_cfg_t cfg16 = { .range = LIS3MDL_RANGE_16G, .addr = LIS3MDL_ADDR_LOW };
    lis3mdl_init(&dev, &model_i2c, &cfg16);
    wait_ms(100);

    lis3mdl_data_t data;
    lis3mdl_err_t err = lis3mdl_read(&dev, &data);
    ASSERT_EQ(err, LIS3MDL_OK);
    ASSERT_NEAR(data.x_ut, 200.0f, 1.0f);
    ASSERT_NEAR(data.y_ut, -100.0f, 1.0f);
    ASSERT_NEAR(data.z_ut, 50.0f, 1.0f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_who_am_i_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, LIS3MDL_REG_WHO_AM_I, 0xFF);
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_WHO_AM_I], 0x3Du);
}

TEST(test_data_registers_readonly)
{
    setup();
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);

    uint8_t orig = model.hw.regs[LIS3MDL_REG_OUT_X_L];
    hw_model__write_reg(&model.hw, LIS3MDL_REG_OUT_X_L, 0xFF);
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_OUT_X_L], orig);
}

TEST(test_ctrl_reg2_writable_mask)
{
    setup();
    /* Table 23, p25: 0 FS1 FS0 0 REBOOT SOFT_RST 0 0. Everything but the
     * two command bits, which act rather than stay. */
    hw_model__write_reg(&model.hw, LIS3MDL_REG_CTRL_REG2, 0xF3);
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG2], 0x60u);
}

TEST(test_ctrl_reg3_writable_mask)
{
    setup();
    /* Table 26, p26: 0 0 LP 0 0 SIM MD1 MD0 */
    hw_model__write_reg(&model.hw, LIS3MDL_REG_CTRL_REG3, 0xFF);
    ASSERT_EQ(model.hw.regs[LIS3MDL_REG_CTRL_REG3], 0x27u);
}

/* SOFT_RST: "Configuration registers and user register reset function",
 * Table 24 on p25 */
TEST(test_soft_rst_restores_the_defaults)
{
    setup();
    write_reg(LIS3MDL_REG_CTRL_REG1, 0x30);
    write_reg(LIS3MDL_REG_CTRL_REG2, 0x60);
    write_reg(LIS3MDL_REG_CTRL_REG3, 0x00);

    write_reg(LIS3MDL_REG_CTRL_REG2, 0x04);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_CTRL_REG1), 0x10);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_CTRL_REG2), 0x00);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_CTRL_REG3), 0x03);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_WHO_AM_I), 0x3D);
}

/* REBOOT reloads memory content, not the user's configuration (p23, p25) */
TEST(test_reboot_leaves_the_configuration)
{
    setup();
    write_reg(LIS3MDL_REG_CTRL_REG1, 0x30);
    write_reg(LIS3MDL_REG_CTRL_REG2, 0x68);
    ASSERT_EQ(model.reboot_count, 1);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_CTRL_REG1), 0x30);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_CTRL_REG2), 0x60);
}

/* Table 16, p23 */
TEST(test_reset_values)
{
    setup();
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_WHO_AM_I), 0x3D);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_CTRL_REG1), 0x10);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_CTRL_REG2), 0x00);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_CTRL_REG3), 0x03);
    ASSERT_EQ_HEX(read_reg(0x23), 0x00);
    ASSERT_EQ_HEX(read_reg(0x24), 0x00);
    ASSERT_EQ_HEX(read_reg(0x30), 0xE8);
    ASSERT_EQ_HEX(read_reg(0x31), 0x00);
}

/* MD = 11 is power-down (Table 28, p26), so nothing is sampled until the
 * driver asks for it, and init has to clear MD */
TEST(test_powers_up_in_power_down)
{
    setup();
    wait_ms(2000);
    ASSERT_EQ(model.measure_count, 0);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_STATUS), 0x00);

    lis3mdl_t dev;
    ASSERT_EQ(lis3mdl_init(&dev, &model_i2c, &default_cfg), LIS3MDL_OK);
    wait_ms(100);
    ASSERT_EQ(model.measure_count, 1);
}

/* "If the MSb of the SUB field is '1', the SUB (register address) is
 * automatically increased", p17 */
TEST(test_sub_msb_decides_the_increment)
{
    setup();
    lis3mdl_t dev;
    ASSERT_EQ(lis3mdl_init(&dev, &model_i2c, &default_cfg), LIS3MDL_OK);
    wait_ms(100);

    uint8_t x_l = (uint8_t)((uint16_t)model.true_x & 0xFFu);
    uint8_t x_h = (uint8_t)((uint16_t)model.true_x >> 8);
    uint8_t buf[3];
    model_i2c.read_regs(&model.hw, LIS3MDL_ADDR_LOW, 0x28, buf, 3);
    ASSERT_EQ_HEX(buf[0], x_l);
    ASSERT_EQ_HEX(buf[1], x_l);
    ASSERT_EQ_HEX(buf[2], x_l);

    model_i2c.read_regs(&model.hw, LIS3MDL_ADDR_LOW, 0xA8, buf, 2);
    ASSERT_EQ_HEX(buf[0], x_l);
    ASSERT_EQ_HEX(buf[1], x_h);
}

/* ZYXDA and the axis flags: "1: a new set of data is available", p28.
 * The first sample comes one period after continuous mode starts. */
TEST(test_status_reg_follows_the_samples)
{
    setup();
    lis3mdl_t dev;
    ASSERT_EQ(lis3mdl_init(&dev, &model_i2c, &default_cfg), LIS3MDL_OK);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_STATUS), 0x00);

    wait_ms(99);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_STATUS), 0x00);
    wait_ms(1);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_STATUS), 0x0F);

    lis3mdl_raw_t raw;
    ASSERT_EQ(lis3mdl_read_raw(&dev, &raw), LIS3MDL_OK);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_STATUS), 0x00);
}

/* ZYXOR: "a new set of data has overwritten the previous set", p28 */
TEST(test_status_reg_reports_overrun)
{
    setup();
    lis3mdl_t dev;
    ASSERT_EQ(lis3mdl_init(&dev, &model_i2c, &default_cfg), LIS3MDL_OK);
    wait_ms(200);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_STATUS), 0xFF);
}

/* Table 22, p25: DO = 101 is 20 Hz, and a sample comes every 50 ms */
TEST(test_samples_follow_the_odr)
{
    setup();
    write_reg(LIS3MDL_REG_CTRL_REG1, 0x14);
    write_reg(LIS3MDL_REG_CTRL_REG3, 0x00);
    wait_ms(49);
    ASSERT_EQ(model.measure_count, 0);
    wait_ms(1);
    ASSERT_EQ(model.measure_count, 1);
    wait_ms(50);
    ASSERT_EQ(model.measure_count, 2);
}

/* BDU: "output registers not updated until MSb and LSb have been read", p27 */
TEST(test_bdu_holds_an_unread_sample)
{
    setup();
    model.true_x = 0x0101;
    write_reg(0x24, 0x40);
    write_reg(LIS3MDL_REG_CTRL_REG3, 0x00);
    wait_ms(100);

    model.true_x = 0x0202;
    wait_ms(100);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_OUT_X_L), 0x01);
}

/* BLE: "1: data MSb at lower address", p26 */
TEST(test_ble_swaps_the_output_bytes)
{
    setup();
    model.true_x = 0x1234;
    write_reg(0x23, 0x02);
    write_reg(LIS3MDL_REG_CTRL_REG3, 0x00);
    wait_ms(100);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_OUT_X_L), 0x12);
    ASSERT_EQ_HEX(read_reg(LIS3MDL_REG_OUT_X_H), 0x34);
}

/* "Writing to those registers may cause permanent damage", p23, and the
 * zero bits "must be set to '0' for correct functioning", p25 and p26 */
TEST(test_model_flags_forbidden_writes)
{
    setup();
    write_reg(0x10, 0xAA);
    ASSERT_EQ(model.reserved_writes, 1);
    ASSERT_EQ_HEX(read_reg(0x10), 0x00);
    write_reg(LIS3MDL_REG_CTRL_REG3, 0x80);
    ASSERT_EQ(model.reserved_writes, 2);
}

TEST(test_init_touches_nothing_forbidden)
{
    static const lis3mdl_range_t ranges[] = {
        LIS3MDL_RANGE_4G, LIS3MDL_RANGE_8G, LIS3MDL_RANGE_12G, LIS3MDL_RANGE_16G,
    };
    for (unsigned i = 0; i < 4; i++) {
        setup();
        lis3mdl_t dev;
        lis3mdl_cfg_t cfg = { .range = ranges[i], .addr = LIS3MDL_ADDR_LOW };
        ASSERT_EQ(lis3mdl_init(&dev, &model_i2c, &cfg), LIS3MDL_OK);
        ASSERT_EQ(model.reserved_writes, 0);
    }
}

TEST(test_different_field_strengths)
{
    setup();
    model.true_x = 100;
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    wait_ms(100);
    lis3mdl_data_t d1;
    lis3mdl_read(&dev, &d1);
    float weak = d1.x_ut;

    model.true_x = 5000;
    wait_ms(100);
    lis3mdl_data_t d2;
    lis3mdl_read(&dev, &d2);
    float strong = d2.x_ut;

    ASSERT_TRUE(strong > weak);
}

TEST(test_continuous_mode_triggers_data)
{
    setup();
    int count_before = model.measure_count;
    lis3mdl_t dev;
    lis3mdl_init(&dev, &model_i2c, &default_cfg);
    /* Init writes CTRL_REG3=0x00, which starts the sample clock */
    ASSERT_EQ(model.measure_count, count_before);
    wait_ms(100);
    ASSERT_TRUE(model.measure_count > count_before);
}

TEST_MAIN("lis3mdl hardware model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_init_wrong_id);
    RUN(test_hw_init_sets_ctrl_reg1);
    RUN(test_hw_init_sets_ctrl_reg2_4g);
    RUN(test_hw_init_sets_ctrl_reg2_8g);
    RUN(test_hw_init_sets_ctrl_reg2_12g);
    RUN(test_hw_init_sets_ctrl_reg2_16g);
    RUN(test_hw_init_sets_continuous_mode);
    RUN(test_hw_read_round_trip);
    RUN(test_read_before_the_first_sample_is_not_ready);
    RUN(test_read_between_samples_stays_ok);
    RUN(test_sample_of_zero_is_a_sample);
    RUN(test_hw_read_ut_4g);
    RUN(test_hw_read_ut_16g);
    RUN(test_who_am_i_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_ctrl_reg2_writable_mask);
    RUN(test_ctrl_reg3_writable_mask);
    RUN(test_soft_rst_restores_the_defaults);
    RUN(test_reboot_leaves_the_configuration);
    RUN(test_reset_values);
    RUN(test_powers_up_in_power_down);
    RUN(test_sub_msb_decides_the_increment);
    RUN(test_status_reg_follows_the_samples);
    RUN(test_status_reg_reports_overrun);
    RUN(test_samples_follow_the_odr);
    RUN(test_bdu_holds_an_unread_sample);
    RUN(test_ble_swaps_the_output_bytes);
    RUN(test_model_flags_forbidden_writes);
    RUN(test_init_touches_nothing_forbidden);
    RUN(test_different_field_strengths);
    RUN(test_continuous_mode_triggers_data);
TEST_MAIN_END()
