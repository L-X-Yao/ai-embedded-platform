// SPDX-License-Identifier: MIT
/*
 * test_hw_model_hmc5883l.c — tests using hardware register model
 *
 * Validates HMC5883L driver against a datasheet-accurate register model.
 * The model enforces read-only ID registers ('H','4','3'), data register
 * protection, config register writable masks, and measurement trigger.
 */

#include "test_harness.h"
#include "hw_model_hmc5883l.h"
#include "../hmc5883l.h"
#include <math.h>

static hmc5883l_model_t model;

static const hmc5883l_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hmc5883l_model_read_regs,
    .delay_ms  = hmc5883l_model_delay_ms,
    .ctx       = &model.hw,
};

static const hmc5883l_i2c_t no_delay_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hmc5883l_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const hmc5883l_cfg_t default_cfg = {
    .gain = HMC5883L_GAIN_1090,
    .rate = HMC5883L_RATE_15,
};

static void setup(void)
{
    hmc5883l_model_init(&model);
}

/* --- bus access, for tests of the model itself -------------------------- */

#define STATUS_RDY  0x01u
#define STATUS_LOCK 0x02u
#define REG_STATUS  0x09u

static void bus_write(uint8_t reg, uint8_t val)
{
    ASSERT_TRUE(hw_model_i2c_write_reg(&model.hw, HMC5883L_ADDR, reg, val));
}

static uint8_t bus_read(uint8_t reg)
{
    uint8_t val = 0xA5u;
    (void)hmc5883l_model_read_regs(&model.hw, HMC5883L_ADDR, reg, &val, 1);
    return val;
}

static void read_data(uint8_t reg, uint16_t len)
{
    uint8_t buf[6];
    ASSERT_TRUE(hmc5883l_model_read_regs(&model.hw, HMC5883L_ADDR, reg, buf, len));
}

static int16_t x_counts(void)
{
    return (int16_t)((uint16_t)model.hw.regs[0x03] << 8 | model.hw.regs[0x04]);
}

static int16_t y_counts(void)
{
    return (int16_t)((uint16_t)model.hw.regs[0x07] << 8 | model.hw.regs[0x08]);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_err_t err = hmc5883l_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, HMC5883L_OK);
}

TEST(test_init_wrong_id)
{
    setup();
    /* Corrupt 'H' -> 'X' */
    model.hw.reset_values[HMC5883L_REG_ID_A] = 'X';
    hw_model_reset(&model.hw);

    hmc5883l_t dev;
    hmc5883l_err_t err = hmc5883l_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, HMC5883L_ERR_ID);
}

TEST(test_init_sets_config_a)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &model_i2c, &cfg);
    /* Config A: rate=4 shifted left by 2 => 0x10 */
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_CONFIG_A], 0x10u);
}

TEST(test_init_sets_config_b)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_1090, .rate = HMC5883L_RATE_15 };
    hmc5883l_init(&dev, &model_i2c, &cfg);
    /* Config B: gain=1 shifted left by 5 => 0x20 */
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_CONFIG_B], 0x20u);
}

TEST(test_init_sets_continuous_mode)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);
    /* Mode register: 0x00 = continuous measurement */
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_MODE], 0x00u);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_raw_values)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);

    hmc5883l_raw_t raw;
    hmc5883l_err_t err = hmc5883l_read_raw(&dev, &raw);
    ASSERT_EQ(err, HMC5883L_OK);
    ASSERT_EQ(raw.x, model.true_x);
    ASSERT_EQ(raw.y, model.true_y);
    ASSERT_EQ(raw.z, model.true_z);
}

TEST(test_read_gauss_reasonable)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);

    hmc5883l_data_t data;
    hmc5883l_err_t err = hmc5883l_read_gauss(&dev, &data);
    ASSERT_EQ(err, HMC5883L_OK);
    /* true_x=500, scale=1/1090 -> ~0.459 Gauss */
    ASSERT_TRUE(data.x_gauss > 0.3f && data.x_gauss < 0.6f);
    /* true_y=-200 -> negative */
    ASSERT_TRUE(data.y_gauss < 0.0f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_id_registers_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, HMC5883L_REG_ID_A, 0xFF);
    hw_model__write_reg(&model.hw, HMC5883L_REG_ID_B, 0xFF);
    hw_model__write_reg(&model.hw, HMC5883L_REG_ID_C, 0xFF);
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_ID_A], 'H');
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_ID_B], '4');
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_ID_C], '3');
}

TEST(test_data_registers_readonly)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);

    uint8_t orig = model.hw.regs[HMC5883L_REG_DATA_X_H];
    hw_model__write_reg(&model.hw, HMC5883L_REG_DATA_X_H, 0xFF);
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_DATA_X_H], orig);
}

TEST(test_config_b_writable_mask)
{
    setup();
    /* Config B only has bits [7:5] writable */
    hw_model__write_reg(&model.hw, HMC5883L_REG_CONFIG_B, 0xFF);
    /* Only high 3 bits should be set */
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_CONFIG_B], 0xE0u);
}

TEST(test_idle_mode)
{
    setup();
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_MODE], 0x00u);

    hmc5883l_idle(&dev);
    ASSERT_EQ(model.hw.regs[HMC5883L_REG_MODE], 0x02u);
}

TEST(test_different_field_strengths)
{
    setup();
    model.true_x = 100;
    hmc5883l_t dev;
    hmc5883l_init(&dev, &model_i2c, &default_cfg);
    hmc5883l_data_t d1;
    hmc5883l_read_gauss(&dev, &d1);
    float weak = d1.x_gauss;

    model.true_x = 800;
    hmc5883l_model_advance_ms(&model, 67);
    hmc5883l_data_t d2;
    hmc5883l_read_gauss(&dev, &d2);
    float strong = d2.x_gauss;

    ASSERT_TRUE(strong > weak);
}

/* --- what the model itself must do (Form 900405 Rev E) ------------------ */

/* p12: "CRA default is 0x10", p13: "CRB default is 0x20", p14: "Mode register
 * default is 0x01", and Status resets with RDY(0) and LOCK (0), p16 */
TEST(test_reset_values)
{
    setup();
    ASSERT_EQ_HEX(bus_read(REG_STATUS), 0x00u);  /* first: reading Mode locks */
    ASSERT_EQ_HEX(bus_read(HMC5883L_REG_CONFIG_A), 0x10u);
    ASSERT_EQ_HEX(bus_read(HMC5883L_REG_CONFIG_B), 0x20u);
    ASSERT_EQ_HEX(bus_read(HMC5883L_REG_MODE), 0x01u);
}

/* p14: "the first measurement set is available from all three data output
 * registers after a period of 2/fDO", and p16: RDY is "Set when data is written
 * to all six data registers". Default 15 Hz: 133.3 ms. */
TEST(test_first_set_takes_two_output_periods)
{
    setup();
    bus_write(HMC5883L_REG_MODE, 0x00u);
    ASSERT_EQ(x_counts(), 0);

    hmc5883l_model_advance_ms(&model, 133);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_RDY, 0u);
    ASSERT_EQ(x_counts(), 0);

    hmc5883l_model_advance_ms(&model, 1);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_RDY, STATUS_RDY);
    ASSERT_EQ(x_counts(), 500);
}

/* p12 Table 5, DO in CRA bits 4:2 */
TEST(test_rate_field_sets_the_wait)
{
    static const struct { uint8_t cra; uint32_t two_periods_ms; } rows[] = {
        { 0x00u, 2667u },  /* DO 000, 2 / 0.75 Hz = 2666.7 ms */
        { 0x0Cu, 267u },   /* DO 011, 2 / 7.5 Hz = 266.7 ms */
        { 0x14u, 67u },    /* DO 101, 2 / 30 Hz = 66.7 ms */
        { 0x18u, 27u },    /* DO 110, 2 / 75 Hz = 26.7 ms */
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        setup();
        bus_write(HMC5883L_REG_CONFIG_A, rows[i].cra);
        bus_write(HMC5883L_REG_MODE, 0x00u);
        hmc5883l_model_advance_ms(&model, rows[i].two_periods_ms - 1u);
        ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_RDY, 0u);
        hmc5883l_model_advance_ms(&model, 1u);
        ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_RDY, STATUS_RDY);
    }
}

/* p14: "subsequent measurements are available at a frequency of fDO", and the
 * registers are updated with each (p10) */
TEST(test_later_sets_come_every_output_period)
{
    setup();
    bus_write(HMC5883L_REG_MODE, 0x00u);
    hmc5883l_model_advance_ms(&model, 134);
    read_data(0x03u, 6);

    model.true_x = 800;
    hmc5883l_model_advance_ms(&model, 66);
    ASSERT_EQ(x_counts(), 500);
    hmc5883l_model_advance_ms(&model, 1);
    ASSERT_EQ(x_counts(), 800);
}

/* p14: the 2/fDO wait follows "a write to the mode or configuration register" */
TEST(test_config_write_restarts_the_wait)
{
    setup();
    bus_write(HMC5883L_REG_MODE, 0x00u);
    hmc5883l_model_advance_ms(&model, 100);
    bus_write(HMC5883L_REG_CONFIG_A, 0x10u);
    hmc5883l_model_advance_ms(&model, 133);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_RDY, 0u);
    hmc5883l_model_advance_ms(&model, 1);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_RDY, STATUS_RDY);
}

/* p13: "the very first measurement after a gain change maintains the same gain
 * as the previous setting. The new gain setting is effective from the second
 * measurement and on." 1000 counts at 1090 LSb/Ga are 0.917 Ga, 403 at 440. */
TEST(test_new_gain_counts_from_the_second_measurement)
{
    setup();
    model.true_x = 1000;
    bus_write(HMC5883L_REG_CONFIG_B, 0x80u);
    bus_write(HMC5883L_REG_MODE, 0x00u);
    hmc5883l_model_advance_ms(&model, 134);
    ASSERT_EQ(x_counts(), 1000);
    read_data(0x03u, 6);
    hmc5883l_model_advance_ms(&model, 67);
    ASSERT_EQ(x_counts(), 403);
}

/* p15: "this data register will contain the value -4096. This register value
 * will clear when after the next valid measurement is made." */
TEST(test_overflow_reads_minus_4096_until_the_next_valid_set)
{
    setup();
    model.true_x = 2100;
    model.true_y = 100;
    bus_write(HMC5883L_REG_MODE, 0x00u);
    hmc5883l_model_advance_ms(&model, 134);
    ASSERT_EQ(x_counts(), -4096);
    ASSERT_EQ(y_counts(), 100);

    read_data(0x03u, 6);
    model.true_x = 100;
    hmc5883l_model_advance_ms(&model, 67);
    ASSERT_EQ(x_counts(), 100);
}

/* p13 Table 9: "Output Range 0xF800-0x07FF (-2048-2047)" */
TEST(test_output_range_limits)
{
    static const struct { int16_t in; int16_t out; } rows[] = {
        { 2047, 2047 }, { 2048, -4096 }, { -2048, -2048 }, { -2049, -4096 },
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        setup();
        model.true_x = rows[i].in;
        bus_write(HMC5883L_REG_MODE, 0x00u);
        hmc5883l_model_advance_ms(&model, 134);
        ASSERT_EQ(x_counts(), rows[i].out);
    }
}

/* p10: in idle mode "All registers maintain values", and nothing is measured */
TEST(test_idle_takes_no_more_sets)
{
    setup();
    bus_write(HMC5883L_REG_MODE, 0x00u);
    hmc5883l_model_advance_ms(&model, 134);
    read_data(0x03u, 6);
    bus_write(HMC5883L_REG_MODE, 0x02u);
    model.true_x = 800;
    hmc5883l_model_advance_ms(&model, 1000);
    ASSERT_EQ(x_counts(), 500);
}

/* p16: "When one or more of the output registers are read, new data cannot be
 * placed in any of the output data registers until all six data output
 * registers are read." LOCK shows it, and p18: "All six data registers must be
 * read properly". */
TEST(test_partial_read_locks_the_data)
{
    setup();
    bus_write(HMC5883L_REG_MODE, 0x00u);
    hmc5883l_model_advance_ms(&model, 134);

    read_data(0x03u, 2);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_LOCK, STATUS_LOCK);
    model.true_x = 800;
    hmc5883l_model_advance_ms(&model, 200);
    ASSERT_EQ(x_counts(), 500);

    read_data(0x05u, 4);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_LOCK, 0u);
    hmc5883l_model_advance_ms(&model, 67);
    ASSERT_EQ(x_counts(), 800);
}

/* p16: LOCK is also set when "Mode register has been read", and ends when "the
 * mode register is changed" or "the measurement configuration (CRA) is
 * changed" */
TEST(test_mode_read_locks_until_a_change)
{
    setup();
    bus_write(HMC5883L_REG_MODE, 0x00u);
    (void)bus_read(HMC5883L_REG_MODE);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_LOCK, STATUS_LOCK);
    bus_write(HMC5883L_REG_MODE, 0x00u);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_LOCK, 0u);

    (void)bus_read(HMC5883L_REG_MODE);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_LOCK, STATUS_LOCK);
    bus_write(HMC5883L_REG_CONFIG_A, 0x10u);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_LOCK, 0u);
}

/* p16: RDY is "Cleared when device initiates a write to the data output
 * registers", and "DRDY and RDY ... cannot be cleared until new data is placed
 * in all the output registers", so in continuous mode a read leaves it set */
TEST(test_reading_the_data_keeps_rdy)
{
    setup();
    bus_write(HMC5883L_REG_MODE, 0x00u);
    hmc5883l_model_advance_ms(&model, 134);
    read_data(0x03u, 6);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_RDY, STATUS_RDY);
}

/* p16: "The status register is an 8-bit read-only register." */
TEST(test_status_is_read_only)
{
    setup();
    bus_write(REG_STATUS, 0xFFu);
    ASSERT_EQ_HEX(bus_read(REG_STATUS), 0x00u);
}

/* p11: "Any attempt to read an invalid address location returns 0's, and any
 * write to an invalid address location ... is ignored" */
TEST(test_invalid_addresses_read_zero)
{
    setup();
    bus_write(0x0Du, 0x5Au);
    bus_write(0x40u, 0x5Au);
    ASSERT_EQ_HEX(bus_read(0x0Du), 0x00u);
    ASSERT_EQ_HEX(bus_read(0x40u), 0x00u);
}

/* --- the driver against the model --------------------------------------- */

/* hmc5883l.h promises "that the first read has a sample at the new gain", and
 * p13 gives the first sample the old gain. RDY stays set across a read by p16,
 * while the Single-Measurement row of p14 has a read clear it. The driver must
 * keep its promise under both, at every rate. */
static void check_first_read_has_the_new_gain(bool read_clears_rdy)
{
    for (uint8_t rate = 0; rate <= (uint8_t)HMC5883L_RATE_75; rate++) {
        setup();
        model.rdy_cleared_by_read = read_clears_rdy;
        model.true_x = 1000;
        hmc5883l_cfg_t cfg = { .gain = HMC5883L_GAIN_440,
                               .rate = (hmc5883l_rate_t)rate };
        hmc5883l_t dev;
        ASSERT_EQ(hmc5883l_init(&dev, &model_i2c, &cfg), HMC5883L_OK);
        hmc5883l_raw_t raw;
        ASSERT_EQ(hmc5883l_read_raw(&dev, &raw), HMC5883L_OK);
        ASSERT_EQ(raw.x, 403);
    }
}

TEST(test_first_read_after_init_has_the_new_gain)
{
    check_first_read_has_the_new_gain(false);
}

TEST(test_first_read_has_the_new_gain_if_a_read_clears_rdy)
{
    check_first_read_has_the_new_gain(true);
}

/* 2/fDO to the first set, then one more period to the second: 200 ms at 15 Hz */
TEST(test_init_waits_three_output_periods)
{
    setup();
    hmc5883l_t dev;
    ASSERT_EQ(hmc5883l_init(&dev, &model_i2c, &default_cfg), HMC5883L_OK);
    ASSERT_GE(model.hw.delay_ms_total, 200u);
}

/* hmc5883l.h: "With NULL, early reads return old data." */
TEST(test_init_without_delay_returns_before_the_first_set)
{
    setup();
    hmc5883l_t dev;
    ASSERT_EQ(hmc5883l_init(&dev, &no_delay_i2c, &default_cfg), HMC5883L_OK);
    hmc5883l_raw_t raw;
    ASSERT_EQ(hmc5883l_read_raw(&dev, &raw), HMC5883L_OK);
    ASSERT_EQ(raw.x, 0);

    hmc5883l_model_advance_ms(&model, 134);
    ASSERT_EQ(hmc5883l_read_raw(&dev, &raw), HMC5883L_OK);
    ASSERT_EQ(raw.x, 500);
}

/* p13: "Use the "Gain" column to convert counts to Gauss." 500 counts at 1090
 * LSb/Ga are 0.4587 Ga, which every gain must report to within one count. */
TEST(test_gauss_matches_the_field_at_every_gain)
{
    static const float lsb_per_gauss[] = {
        1370.0f, 1090.0f, 820.0f, 660.0f, 440.0f, 390.0f, 330.0f, 230.0f,
    };
    for (uint8_t gain = 0; gain <= (uint8_t)HMC5883L_GAIN_230; gain++) {
        setup();
        hmc5883l_cfg_t cfg = { .gain = (hmc5883l_gain_t)gain,
                               .rate = HMC5883L_RATE_15 };
        hmc5883l_t dev;
        ASSERT_EQ(hmc5883l_init(&dev, &model_i2c, &cfg), HMC5883L_OK);
        hmc5883l_data_t data;
        ASSERT_EQ(hmc5883l_read_gauss(&dev, &data), HMC5883L_OK);
        ASSERT_NEAR(data.x_gauss, 500.0f / 1090.0f, 1.0f / lsb_per_gauss[gain]);
    }
}

TEST(test_driver_reports_overflow)
{
    setup();
    model.true_z = -3000;
    hmc5883l_t dev;
    ASSERT_EQ(hmc5883l_init(&dev, &model_i2c, &default_cfg), HMC5883L_OK);
    hmc5883l_raw_t raw;
    ASSERT_EQ(hmc5883l_read_raw(&dev, &raw), HMC5883L_ERR_OVERFLOW);
}

/* The driver reads all six bytes, so it never leaves the data locked */
TEST(test_driver_reads_leave_no_lock)
{
    setup();
    hmc5883l_t dev;
    ASSERT_EQ(hmc5883l_init(&dev, &model_i2c, &default_cfg), HMC5883L_OK);
    hmc5883l_raw_t raw;
    ASSERT_EQ(hmc5883l_read_raw(&dev, &raw), HMC5883L_OK);
    ASSERT_EQ_HEX(bus_read(REG_STATUS) & STATUS_LOCK, 0u);
}

TEST(test_driver_idle_stops_the_sets)
{
    setup();
    hmc5883l_t dev;
    ASSERT_EQ(hmc5883l_init(&dev, &model_i2c, &default_cfg), HMC5883L_OK);
    ASSERT_EQ(hmc5883l_idle(&dev), HMC5883L_OK);
    int before = model.measure_count;
    hmc5883l_model_advance_ms(&model, 1000);
    ASSERT_EQ(model.measure_count, before);
}

TEST_MAIN("hmc5883l hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_sets_config_a);
    RUN(test_init_sets_config_b);
    RUN(test_init_sets_continuous_mode);
    RUN(test_read_raw_values);
    RUN(test_read_gauss_reasonable);
    RUN(test_id_registers_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_config_b_writable_mask);
    RUN(test_idle_mode);
    RUN(test_different_field_strengths);
    RUN(test_reset_values);
    RUN(test_first_set_takes_two_output_periods);
    RUN(test_rate_field_sets_the_wait);
    RUN(test_later_sets_come_every_output_period);
    RUN(test_config_write_restarts_the_wait);
    RUN(test_new_gain_counts_from_the_second_measurement);
    RUN(test_overflow_reads_minus_4096_until_the_next_valid_set);
    RUN(test_output_range_limits);
    RUN(test_idle_takes_no_more_sets);
    RUN(test_partial_read_locks_the_data);
    RUN(test_mode_read_locks_until_a_change);
    RUN(test_reading_the_data_keeps_rdy);
    RUN(test_status_is_read_only);
    RUN(test_invalid_addresses_read_zero);
    RUN(test_first_read_after_init_has_the_new_gain);
    RUN(test_first_read_has_the_new_gain_if_a_read_clears_rdy);
    RUN(test_init_waits_three_output_periods);
    RUN(test_init_without_delay_returns_before_the_first_set);
    RUN(test_gauss_matches_the_field_at_every_gain);
    RUN(test_driver_reports_overflow);
    RUN(test_driver_reads_leave_no_lock);
    RUN(test_driver_idle_stops_the_sets);
TEST_MAIN_END()
