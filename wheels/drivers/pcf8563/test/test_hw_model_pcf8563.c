// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "hw_model_pcf8563.h"
#include "pcf8563.h"

static const pcf8563_i2c_t hw_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .ctx       = NULL,
};

static pcf8563_t dev;

TEST(test_hw_init_por_defaults)
{
    hw_model_reset();
    ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);
}

TEST(test_hw_read_planted_time)
{
    hw_model_reset();
    pcf8563_init(&dev, &hw_i2c);
    hw_model_inject_time(26, 3, 20, 5, 14, 30, 0);

    pcf8563_time_t t;
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_OK);
    /* 2026-03-20 Fri 14:30:00 */
    ASSERT_EQ(t.year, 26);
    ASSERT_EQ(t.month, 3);
    ASSERT_EQ(t.day, 20);
    ASSERT_EQ(t.weekday, 5);
    ASSERT_EQ(t.hour, 14);
    ASSERT_EQ(t.minute, 30);
    ASSERT_EQ(t.second, 0);
}

TEST(test_hw_write_read_roundtrip)
{
    hw_model_reset();
    pcf8563_init(&dev, &hw_i2c);

    pcf8563_time_t tw = {
        .year = 99, .month = 12, .day = 31,
        .weekday = 6, .hour = 23, .minute = 59, .second = 59
    };
    ASSERT_EQ(pcf8563_write_time(&dev, &tw), PCF8563_OK);

    pcf8563_time_t tr;
    ASSERT_EQ(pcf8563_read_time(&dev, &tr), PCF8563_OK);
    ASSERT_EQ(tr.year, 99);
    ASSERT_EQ(tr.month, 12);
    ASSERT_EQ(tr.day, 31);
    ASSERT_EQ(tr.weekday, 6);
    ASSERT_EQ(tr.hour, 23);
    ASSERT_EQ(tr.minute, 59);
    ASSERT_EQ(tr.second, 59);
}

/* Rev. 10 p.20: with STOP set the time circuits "will not increment until the
 * STOP bit is released". The model reads the N bits as 1, and Table 4 wants
 * them written as 0, so Control_status_1 must end at 0x00. */
TEST(test_hw_write_time_runs_stopped)
{
    hw_model_reset();
    pcf8563_init(&dev, &hw_i2c);

    pcf8563_time_t t = {
        .year = 26, .month = 11, .day = 6,
        .weekday = 5, .hour = 12, .minute = 30, .second = 15
    };
    ASSERT_EQ(pcf8563_write_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ(hw_time_writes_running, 0);
    ASSERT_EQ_HEX(hw_regs[0x00], 0x00u);
}

TEST(test_hw_inject_time)
{
    hw_model_reset();
    pcf8563_init(&dev, &hw_i2c);

    hw_model_inject_time(25, 6, 15, 0, 8, 5, 3);

    pcf8563_time_t t;
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ(t.year, 25);
    ASSERT_EQ(t.month, 6);
    ASSERT_EQ(t.day, 15);
    ASSERT_EQ(t.weekday, 0);
    ASSERT_EQ(t.hour, 8);
    ASSERT_EQ(t.minute, 5);
    ASSERT_EQ(t.second, 3);
}

TEST(test_hw_alarm_roundtrip)
{
    hw_model_reset();
    pcf8563_init(&dev, &hw_i2c);

    pcf8563_alarm_t aw = {
        .minute = 45, .hour = 6,
        .day = 15, .weekday = 0x80
    };
    ASSERT_EQ(pcf8563_write_alarm(&dev, &aw), PCF8563_OK);

    pcf8563_alarm_t ar;
    ASSERT_EQ(pcf8563_read_alarm(&dev, &ar), PCF8563_OK);
    ASSERT_EQ(ar.minute, 45);
    ASSERT_EQ(ar.hour, 6);
    ASSERT_EQ(ar.day, 15);
    ASSERT_EQ(ar.weekday, 0x80);
}

/* --- what Rev. 10 fixes -------------------------------------------------- */

#define ADDR 0x51

static uint8_t read_one(uint8_t reg)
{
    uint8_t val = 0;

    (void)hw_model_read_regs(NULL, ADDR, reg, &val, 1);
    return val;
}

/* Table 27. Bits marked x "are undefined at power-up". */
TEST(test_hw_reset_values)
{
    hw_model_reset();
    ASSERT_EQ_HEX(hw_regs[0x00], 0x08);         /* TESTC = 1 */
    ASSERT_EQ_HEX(hw_regs[0x01], 0x00);
    ASSERT_EQ_HEX(hw_regs[0x02] & 0x80u, 0x80); /* VL = 1 */
    for (uint8_t reg = 0x09; reg <= 0x0C; reg++)
        ASSERT_EQ_HEX(hw_regs[reg] & 0x80u, 0x80);  /* AE_x = 1 */
    ASSERT_EQ_HEX(hw_regs[0x0D] & 0x83u, 0x80); /* FE = 1, FD = 00 */
    ASSERT_EQ_HEX(hw_regs[0x0E] & 0x83u, 0x03); /* TE = 0, TD = 11 */
}

/* Table 4: x bits "are not relevant", and N bits "could be either logic 0 or
 * logic 1". The model reads them all as 1. */
TEST(test_hw_unused_bits_read_1)
{
    static const struct { uint8_t reg; uint8_t bits; } cases[] = {
        { 0x00, 0x57 }, { 0x01, 0xE0 }, { 0x03, 0x80 }, { 0x04, 0xC0 },
        { 0x05, 0xC0 }, { 0x06, 0xF8 }, { 0x07, 0x60 }, { 0x0A, 0x40 },
        { 0x0B, 0x40 }, { 0x0C, 0x78 }, { 0x0D, 0x7C }, { 0x0E, 0x7C },
    };

    hw_model_reset();
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        hw_regs[cases[i].reg] = 0x00;
        ASSERT_EQ_HEX(read_one(cases[i].reg), cases[i].bits);
    }
}

/* Table 4: "Bit positions labelled with N should always be written with logic
 * 0" */
TEST(test_hw_n_bits_are_counted_when_written_as_1)
{
    hw_model_reset();
    hw_model_write_reg(NULL, ADDR, 0x00, 0x40);
    ASSERT_EQ(hw_n_bits_written, 1);
    hw_model_write_reg(NULL, ADDR, 0x01, 0x20);
    ASSERT_EQ(hw_n_bits_written, 2);
}

TEST(test_hw_driver_writes_n_bits_as_0)
{
    pcf8563_time_t t = {
        .year = 26, .month = 11, .day = 6,
        .weekday = 5, .hour = 12, .minute = 30, .second = 15
    };

    hw_model_reset();
    ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);
    ASSERT_EQ(pcf8563_write_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ(hw_n_bits_written, 0);
}

/* p.9: "To prevent one flag being overwritten while clearing another, a logic
 * AND is performed during a write access." */
TEST(test_hw_af_and_tf_are_anded_on_write)
{
    hw_model_reset();
    hw_regs[0x01] = 0x0C;                        /* AF and TF */
    hw_model_write_reg(NULL, ADDR, 0x01, 0x08);  /* clear TF only */
    ASSERT_EQ_HEX(hw_regs[0x01], 0x08);

    hw_regs[0x01] = 0x00;
    hw_model_write_reg(NULL, ADDR, 0x01, 0x0C);  /* a write cannot raise them */
    ASSERT_EQ_HEX(hw_regs[0x01], 0x00);

    hw_model_write_reg(NULL, ADDR, 0x01, 0x03);  /* AIE and TIE are plain bits */
    ASSERT_EQ_HEX(hw_regs[0x01], 0x03);
}

/* p.22: the time registers are undefined at power-up, with VL set. Here they
 * read all ones, which decodes to a date like 20165-25-45. */
TEST(test_hw_first_power_up_reports_vl)
{
    hw_model_reset();
    ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);

    pcf8563_time_t t;
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_ERR_VL);
    ASSERT_EQ(t.year, 165);
    ASSERT_EQ(t.month, 25);
    ASSERT_EQ(t.day, 45);

    const pcf8563_time_t set = {
        .year = 26, .month = 11, .day = 6,
        .weekday = 5, .hour = 12, .minute = 30, .second = 15
    };
    ASSERT_EQ(pcf8563_write_time(&dev, &set), PCF8563_OK);
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ(t.year, 26);
    ASSERT_EQ(t.second, 15);
}

/* p.10: "The VL flag can only be cleared by using the interface." */
TEST(test_hw_vl_stays_set_until_written)
{
    pcf8563_time_t t = {
        .year = 26, .month = 11, .day = 6,
        .weekday = 5, .hour = 12, .minute = 30, .second = 15
    };

    hw_model_reset();
    hw_model_inject_time(26, 11, 6, 5, 12, 30, 15);
    hw_regs[0x02] |= 0x80u;
    ASSERT_EQ_HEX(read_one(0x02), 0x95);
    hw_model_tick(1);
    ASSERT_EQ_HEX(read_one(0x02), 0x96);

    ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);
    ASSERT_EQ(pcf8563_write_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ_HEX(read_one(0x02), 0x15);
}

/* p.20: with STOP "no 1 Hz ticks will be generated" */
TEST(test_hw_stop_holds_the_time_circuits)
{
    hw_model_reset();
    ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);
    hw_model_inject_time(26, 11, 6, 5, 12, 30, 15);

    hw_model_write_reg(NULL, ADDR, 0x00, 0x20);
    hw_model_tick(5);
    ASSERT_EQ_HEX(hw_regs[0x02], 0x15);

    hw_model_write_reg(NULL, ADDR, 0x00, 0x00);
    hw_model_tick(5);
    ASSERT_EQ_HEX(hw_regs[0x02], 0x20);
}

/* Figure 8 and p.12: every counter carries into the next, and "This bit is
 * toggled when the register Years overflows from 99 to 00." */
TEST(test_hw_time_rolls_over_at_the_end_of_the_century)
{
    hw_model_reset();
    ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);
    hw_model_inject_time(99, 12, 31, 6, 23, 59, 59);

    hw_model_tick(1);
    pcf8563_time_t t;
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ(t.year, 0);
    ASSERT_EQ(t.month, 1);
    ASSERT_EQ(t.day, 1);
    ASSERT_EQ(t.weekday, 0);
    ASSERT_EQ(t.hour, 0);
    ASSERT_EQ(t.minute, 0);
    ASSERT_EQ(t.second, 0);
    ASSERT_EQ_HEX(hw_regs[0x07], 0x81);
}

/* p.11: "a 29th day to February if the year counter contains a value which is
 * exactly divisible by 4, including the year 00" */
TEST(test_hw_february_follows_the_leap_rule)
{
    static const struct { uint8_t year; uint8_t month; uint8_t day; } cases[] = {
        { 24, 2, 29 }, { 0, 2, 29 }, { 25, 3, 1 }, { 98, 3, 1 },
    };

    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        hw_model_reset();
        ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);
        hw_model_inject_time(cases[i].year, 2, 28, 3, 23, 59, 59);

        hw_model_tick(1);
        pcf8563_time_t t;
        ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_OK);
        ASSERT_EQ(t.month, cases[i].month);
        ASSERT_EQ(t.day, cases[i].day);
    }
}

/* p.16: "When all enabled comparisons first match, the alarm flag (AF in
 * register Control_2) is set to logic 1", and once cleared "it will only be
 * set again when the time increments to match the alarm condition once
 * more". */
TEST(test_hw_alarm_fires_when_the_enabled_fields_match)
{
    pcf8563_alarm_t alarm = {
        .minute = 45, .hour = 16, .day = 0x80, .weekday = 0x80
    };

    hw_model_reset();
    ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);
    ASSERT_EQ(pcf8563_write_alarm(&dev, &alarm), PCF8563_OK);

    hw_model_inject_time(26, 11, 6, 5, 16, 44, 58);
    hw_model_tick(1);
    ASSERT_EQ_HEX(hw_regs[0x01] & 0x08u, 0x00);
    hw_model_tick(1);
    ASSERT_EQ_HEX(hw_regs[0x01] & 0x08u, 0x08);

    hw_model_write_reg(NULL, ADDR, 0x01, 0x00);
    hw_model_tick(1);
    ASSERT_EQ_HEX(hw_regs[0x01] & 0x08u, 0x00);
}

/* p.16: "Alarm registers which have their AE_x bit at logic 1 are ignored." */
TEST(test_hw_no_alarm_with_every_ae_bit_set)
{
    hw_model_reset();
    ASSERT_EQ(pcf8563_init(&dev, &hw_i2c), PCF8563_OK);
    hw_model_inject_time(26, 11, 6, 5, 16, 44, 58);
    hw_model_tick(120);
    ASSERT_EQ_HEX(hw_regs[0x01] & 0x08u, 0x00);
}

TEST_MAIN("pcf8563_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_planted_time);
    RUN(test_hw_write_read_roundtrip);
    RUN(test_hw_write_time_runs_stopped);
    RUN(test_hw_inject_time);
    RUN(test_hw_alarm_roundtrip);
    RUN(test_hw_reset_values);
    RUN(test_hw_unused_bits_read_1);
    RUN(test_hw_n_bits_are_counted_when_written_as_1);
    RUN(test_hw_driver_writes_n_bits_as_0);
    RUN(test_hw_af_and_tf_are_anded_on_write);
    RUN(test_hw_first_power_up_reports_vl);
    RUN(test_hw_vl_stays_set_until_written);
    RUN(test_hw_stop_holds_the_time_circuits);
    RUN(test_hw_time_rolls_over_at_the_end_of_the_century);
    RUN(test_hw_february_follows_the_leap_rule);
    RUN(test_hw_alarm_fires_when_the_enabled_fields_match);
    RUN(test_hw_no_alarm_with_every_ae_bit_set);
TEST_MAIN_END()
