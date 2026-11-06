// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "pcf8563.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* Rev. 10 Table 27. Registers marked x "are undefined at power-up" and
     * read 1 here, which leaves VL = 1 and AE_x = 1 (alarm disabled). */
    memset(mock_regs, 0xFF, 16);
    mock_regs[0x00] = 0x08;   /* Control_status_1: TESTC = 1 */
    mock_regs[0x01] = 0x00;   /* Control_status_2 */
    mock_regs[0x0D] = 0x80;   /* CLKOUT_control: FE = 1, FD = 00 */
    mock_regs[0x0E] = 0x03;   /* Timer_control: TE = 0, TD = 11 */
}

/* 2026-01-15 Thu 12:30:45 in BCD */
static void plant_time(void)
{
    mock_regs[0x02] = 0x45;   /* seconds: 45 = 0x45 BCD */
    mock_regs[0x03] = 0x30;   /* minutes: 30 = 0x30 BCD */
    mock_regs[0x04] = 0x12;   /* hours:   12 = 0x12 BCD */
    mock_regs[0x05] = 0x15;   /* days:    15 = 0x15 BCD */
    mock_regs[0x06] = 0x04;   /* weekday: 4 = Thursday */
    mock_regs[0x07] = 0x01;   /* month:   01 = 0x01 BCD */
    mock_regs[0x08] = 0x26;   /* year:    26 = 0x26 BCD */
}

static const pcf8563_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static pcf8563_t dev;

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(pcf8563_init(&dev, &mock_i2c), PCF8563_OK);
}

/* Control_status_1 as read back after init writes 0x00. Rev. 10 Table 4: N
 * bits "could be either logic 0 or logic 1". Table 5: bit 7 TEST1, bit 5 STOP. */
static uint8_t ctrl1_or_bits;

static bool read_regs_ctrl1_bits(void *ctx, uint8_t dev_addr, uint8_t reg,
                                 uint8_t *buf, uint16_t len)
{
    bool ok = mock_i2c_read_regs(ctx, dev_addr, reg, buf, len);
    if (ok && reg == 0x00u) buf[0] |= ctrl1_or_bits;
    return ok;
}

static const pcf8563_i2c_t mock_i2c_ctrl1 = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = read_regs_ctrl1_bits,
    .ctx       = NULL,
};

TEST(test_init_accepts_n_bits_high)
{
    mock_reset();
    ctrl1_or_bits = 0x57u;   /* bits 6, 4 and 2-0 */
    ASSERT_EQ(pcf8563_init(&dev, &mock_i2c_ctrl1), PCF8563_OK);
    ctrl1_or_bits = 0x40u;   /* bit 6 alone */
    ASSERT_EQ(pcf8563_init(&dev, &mock_i2c_ctrl1), PCF8563_OK);
}

TEST(test_init_refuses_test1_or_stop)
{
    mock_reset();
    ctrl1_or_bits = 0x80u;   /* TEST1 */
    ASSERT_EQ(pcf8563_init(&dev, &mock_i2c_ctrl1), PCF8563_ERR_ID);
    ctrl1_or_bits = 0x20u;   /* STOP */
    ASSERT_EQ(pcf8563_init(&dev, &mock_i2c_ctrl1), PCF8563_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(pcf8563_init(&dev, &mock_i2c), PCF8563_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(pcf8563_init(NULL, &mock_i2c), PCF8563_ERR_PARAM);
    ASSERT_EQ(pcf8563_init(&dev, NULL), PCF8563_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    /* Zero HAL struct has NULL function pointers -- init must reject it */
    pcf8563_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    pcf8563_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(pcf8563_init(&zero_dev, &zero_hal) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)PCF8563_ERR_I2C);
    ASSERT_EQ(-2, (int)PCF8563_ERR_ID);
    ASSERT_EQ(-3, (int)PCF8563_ERR_PARAM);
}

TEST(test_read_time)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);
    plant_time();

    pcf8563_time_t t;
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ(t.second, 45);
    ASSERT_EQ(t.minute, 30);
    ASSERT_EQ(t.hour, 12);
    ASSERT_EQ(t.day, 15);
    ASSERT_EQ(t.weekday, 4);
    ASSERT_EQ(t.month, 1);
    ASSERT_EQ(t.year, 26);
}

TEST(test_read_time_null)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);
    ASSERT_EQ(pcf8563_read_time(NULL, &(pcf8563_time_t){0}), PCF8563_ERR_PARAM);
    ASSERT_EQ(pcf8563_read_time(&dev, NULL), PCF8563_ERR_PARAM);
}

TEST(test_read_time_i2c_fail)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);
    mock_fail_at = mock_op_count;
    pcf8563_time_t t;
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_ERR_I2C);
}

TEST(test_write_time)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    pcf8563_time_t t = {
        .year = 25, .month = 12, .day = 31,
        .weekday = 3, .hour = 23, .minute = 59, .second = 58
    };
    ASSERT_EQ(pcf8563_write_time(&dev, &t), PCF8563_OK);

    /* Verify BCD encoding in mock registers */
    ASSERT_EQ(mock_regs[0x02], 0x58);  /* 58 seconds */
    ASSERT_EQ(mock_regs[0x03], 0x59);  /* 59 minutes */
    ASSERT_EQ(mock_regs[0x04], 0x23);  /* 23 hours */
    ASSERT_EQ(mock_regs[0x05], 0x31);  /* 31 days */
    ASSERT_EQ(mock_regs[0x06], 0x03);  /* weekday 3 */
    ASSERT_EQ(mock_regs[0x07], 0x12);  /* 12 months = 0x12 BCD */
    ASSERT_EQ(mock_regs[0x08], 0x25);  /* 25 years = 0x25 BCD */
}

/* Every write_reg call in order, with Control_status_1 as it stood when the
 * write arrived. Rev. 10 Table 5: TEST1 bit 7, STOP bit 5, TESTC bit 3. */
#define WLOG_MAX 16
static struct { uint8_t reg; uint8_t val; uint8_t ctrl1; bool ok; } wlog[WLOG_MAX];
static int wlog_n;

static bool write_reg_logged(void *ctx, uint8_t dev_addr, uint8_t reg,
                             uint8_t val)
{
    uint8_t ctrl1 = mock_regs[0x00];
    bool ok = mock_i2c_write_reg(ctx, dev_addr, reg, val);
    if (wlog_n < WLOG_MAX) {
        wlog[wlog_n].reg   = reg;
        wlog[wlog_n].val   = val;
        wlog[wlog_n].ctrl1 = ctrl1;
        wlog[wlog_n].ok    = ok;
        wlog_n++;
    }
    return ok;
}

static const pcf8563_i2c_t mock_i2c_logged = {
    .write_reg = write_reg_logged,
    .read_regs = read_regs_ctrl1_bits,
    .ctx       = NULL,
};

static const pcf8563_time_t stop_test_time = {
    .year = 26, .month = 11, .day = 6,
    .weekday = 5, .hour = 12, .minute = 30, .second = 15
};

static void stop_test_setup(void)
{
    mock_reset();
    ctrl1_or_bits = 0;
    (void)pcf8563_init(&dev, &mock_i2c_logged);
    wlog_n = 0;
}

/* Rev. 10 p.20: with STOP set "The time circuits can then be set and will not
 * increment until the STOP bit is released". */
TEST(test_write_time_holds_stop_for_every_time_write)
{
    stop_test_setup();
    ASSERT_EQ(pcf8563_write_time(&dev, &stop_test_time), PCF8563_OK);

    ASSERT_EQ(wlog_n, 9);
    ASSERT_EQ(wlog[0].reg, 0x00);
    ASSERT_EQ_HEX(wlog[0].val, 0x20u);
    for (int i = 0; i < 7; i++) {
        ASSERT_EQ(wlog[1 + i].reg, 0x02 + i);
        ASSERT_EQ_HEX(wlog[1 + i].ctrl1 & 0x20u, 0x20u);
    }
    ASSERT_EQ(wlog[8].reg, 0x00);
    ASSERT_EQ_HEX(wlog[8].val, 0x00u);
    ASSERT_EQ_HEX(mock_regs[0x00], 0x00u);
}

/* TEST1 and TESTC keep their values. N bits may read 1 but Table 4 says they
 * "should always be written with logic 0". */
TEST(test_write_time_keeps_control_bits)
{
    stop_test_setup();
    mock_regs[0x00] = 0x88u;   /* TEST1, TESTC */
    ctrl1_or_bits   = 0x57u;   /* bits 6, 4 and 2-0 */
    ASSERT_EQ(pcf8563_write_time(&dev, &stop_test_time), PCF8563_OK);

    ASSERT_EQ(wlog_n, 9);
    ASSERT_EQ_HEX(wlog[0].val, 0xA8u);
    ASSERT_EQ_HEX(wlog[8].val, 0x88u);
}

/* A failed time write stops the sequence but must not leave the clock stopped */
TEST(test_write_time_failure_still_releases_stop)
{
    for (int k = 0; k < 7; k++) {
        stop_test_setup();
        mock_fail_at = mock_op_count + 2 + k;  /* read, STOP, k good writes */
        ASSERT_EQ(pcf8563_write_time(&dev, &stop_test_time), PCF8563_ERR_I2C);

        ASSERT_EQ(wlog_n, k + 3);              /* STOP, k + 1 tries, release */
        ASSERT_FALSE(wlog[k + 1].ok);
        ASSERT_EQ(wlog[k + 2].reg, 0x00);
        ASSERT_TRUE(wlog[k + 2].ok);
        ASSERT_EQ_HEX(mock_regs[0x00], 0x00u);
    }
}

TEST(test_write_time_stop_write_fails)
{
    stop_test_setup();
    mock_fail_at = mock_op_count + 1;
    ASSERT_EQ(pcf8563_write_time(&dev, &stop_test_time), PCF8563_ERR_I2C);

    /* No time write without STOP, and the release is still tried */
    ASSERT_EQ(wlog_n, 2);
    ASSERT_EQ(wlog[0].reg, 0x00);
    ASSERT_EQ(wlog[1].reg, 0x00);
    ASSERT_EQ_HEX(wlog[1].val, 0x00u);
}

TEST(test_write_time_ctrl1_read_fails)
{
    stop_test_setup();
    mock_fail_at = mock_op_count;
    ASSERT_EQ(pcf8563_write_time(&dev, &stop_test_time), PCF8563_ERR_I2C);
    ASSERT_EQ(wlog_n, 0);
}

TEST(test_write_time_release_fails)
{
    stop_test_setup();
    mock_fail_at = mock_op_count + 9;
    ASSERT_EQ(pcf8563_write_time(&dev, &stop_test_time), PCF8563_ERR_I2C);
    ASSERT_EQ(wlog_n, 9);
    ASSERT_EQ_HEX(mock_regs[0x00], 0x20u);     /* the clock stays stopped */
}

TEST(test_write_time_null)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);
    ASSERT_EQ(pcf8563_write_time(NULL, &(pcf8563_time_t){0}), PCF8563_ERR_PARAM);
    ASSERT_EQ(pcf8563_write_time(&dev, NULL), PCF8563_ERR_PARAM);
}

TEST(test_write_time_invalid)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    /* month=0 is invalid */
    pcf8563_time_t bad = {.year=0, .month=0, .day=1, .weekday=0,
                          .hour=0, .minute=0, .second=0};
    ASSERT_EQ(pcf8563_write_time(&dev, &bad), PCF8563_ERR_PARAM);

    /* day=32 is invalid */
    bad.month = 1; bad.day = 32;
    ASSERT_EQ(pcf8563_write_time(&dev, &bad), PCF8563_ERR_PARAM);

    /* hour=24 is invalid */
    bad.day = 1; bad.hour = 24;
    ASSERT_EQ(pcf8563_write_time(&dev, &bad), PCF8563_ERR_PARAM);

    /* minute=60 is invalid */
    bad.hour = 0; bad.minute = 60;
    ASSERT_EQ(pcf8563_write_time(&dev, &bad), PCF8563_ERR_PARAM);

    /* second=60 is invalid */
    bad.minute = 0; bad.second = 60;
    ASSERT_EQ(pcf8563_write_time(&dev, &bad), PCF8563_ERR_PARAM);

    /* weekday=7 is invalid */
    bad.second = 0; bad.weekday = 7;
    ASSERT_EQ(pcf8563_write_time(&dev, &bad), PCF8563_ERR_PARAM);

    /* year=100 is invalid */
    bad.weekday = 0; bad.year = 100;
    ASSERT_EQ(pcf8563_write_time(&dev, &bad), PCF8563_ERR_PARAM);
}

TEST(test_time_roundtrip)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    pcf8563_time_t tw = {
        .year = 26, .month = 6, .day = 15,
        .weekday = 1, .hour = 8, .minute = 5, .second = 3
    };
    ASSERT_EQ(pcf8563_write_time(&dev, &tw), PCF8563_OK);

    pcf8563_time_t tr;
    ASSERT_EQ(pcf8563_read_time(&dev, &tr), PCF8563_OK);
    ASSERT_EQ(tr.year, 26);
    ASSERT_EQ(tr.month, 6);
    ASSERT_EQ(tr.day, 15);
    ASSERT_EQ(tr.weekday, 1);
    ASSERT_EQ(tr.hour, 8);
    ASSERT_EQ(tr.minute, 5);
    ASSERT_EQ(tr.second, 3);
}

TEST(test_bcd_encoding_boundary)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    /* Write 09:09:09 -- single digit BCD should have upper nibble 0 */
    pcf8563_time_t t = {
        .year = 9, .month = 9, .day = 9,
        .weekday = 0, .hour = 9, .minute = 9, .second = 9
    };
    ASSERT_EQ(pcf8563_write_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ(mock_regs[0x02], 0x09);
    ASSERT_EQ(mock_regs[0x03], 0x09);
    ASSERT_EQ(mock_regs[0x04], 0x09);
}

TEST(test_vl_bit_masked_on_read)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    /* Set VL (voltage-low) bit in seconds register */
    mock_regs[0x02] = 0x80 | 0x45;  /* VL set + 45 sec BCD */

    pcf8563_time_t t;
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_OK);
    ASSERT_EQ(t.second, 45);  /* VL bit must be masked */
}

TEST(test_alarm_write_read)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    pcf8563_alarm_t aw = {
        .minute = 30, .hour = 7,
        .day = 0x80, .weekday = 0x80  /* day/weekday disabled */
    };
    ASSERT_EQ(pcf8563_write_alarm(&dev, &aw), PCF8563_OK);

    pcf8563_alarm_t ar;
    ASSERT_EQ(pcf8563_read_alarm(&dev, &ar), PCF8563_OK);
    ASSERT_EQ(ar.minute, 30);
    ASSERT_EQ(ar.hour, 7);
    ASSERT_EQ(ar.day, 0x80);
    ASSERT_EQ(ar.weekday, 0x80);
}

TEST(test_alarm_all_disabled)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    pcf8563_alarm_t aw = {
        .minute = 0x80, .hour = 0x80,
        .day = 0x80, .weekday = 0x80
    };
    ASSERT_EQ(pcf8563_write_alarm(&dev, &aw), PCF8563_OK);

    /* Verify AE bits are set in registers */
    ASSERT_TRUE(mock_regs[0x09] & 0x80);
    ASSERT_TRUE(mock_regs[0x0A] & 0x80);
    ASSERT_TRUE(mock_regs[0x0B] & 0x80);
    ASSERT_TRUE(mock_regs[0x0C] & 0x80);
}

TEST(test_alarm_null)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);
    ASSERT_EQ(pcf8563_read_alarm(NULL, &(pcf8563_alarm_t){0}), PCF8563_ERR_PARAM);
    ASSERT_EQ(pcf8563_read_alarm(&dev, NULL), PCF8563_ERR_PARAM);
    ASSERT_EQ(pcf8563_write_alarm(NULL, &(pcf8563_alarm_t){0}), PCF8563_ERR_PARAM);
    ASSERT_EQ(pcf8563_write_alarm(&dev, NULL), PCF8563_ERR_PARAM);
}

TEST(test_write_alarm_invalid)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    /* minute > 59 with AE bit clear = invalid */
    pcf8563_alarm_t bad = {.minute = 60, .hour = 0x80, .day = 0x80, .weekday = 0x80};
    ASSERT_EQ(pcf8563_write_alarm(&dev, &bad), PCF8563_ERR_PARAM);

    /* hour > 23 with AE bit clear = invalid */
    bad.minute = 0; bad.hour = 24;
    ASSERT_EQ(pcf8563_write_alarm(&dev, &bad), PCF8563_ERR_PARAM);

    /* day = 0 with AE bit clear = invalid (day must be 1-31) */
    bad.hour = 0x80; bad.day = 0;
    ASSERT_EQ(pcf8563_write_alarm(&dev, &bad), PCF8563_ERR_PARAM);

    /* day > 31 with AE bit clear = invalid */
    bad.day = 32;
    ASSERT_EQ(pcf8563_write_alarm(&dev, &bad), PCF8563_ERR_PARAM);

    /* weekday > 6 with AE bit clear = invalid */
    bad.day = 0x80; bad.weekday = 7;
    ASSERT_EQ(pcf8563_write_alarm(&dev, &bad), PCF8563_ERR_PARAM);
}

TEST(test_timer_enable)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    ASSERT_EQ(pcf8563_enable_timer(&dev, PCF8563_TIMER_1HZ, 60), PCF8563_OK);
    ASSERT_EQ(mock_regs[0x0F], 60);          /* timer value */
    ASSERT_EQ(mock_regs[0x0E], 0x80 | 0x02); /* TE=1, TD=1Hz */
}

TEST(test_timer_disable)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    pcf8563_enable_timer(&dev, PCF8563_TIMER_1HZ, 100);
    ASSERT_EQ(pcf8563_disable_timer(&dev), PCF8563_OK);
    /* Rev. 10 Table 23: TE = 0, and when not in use TD[1:0] should be
     * 11 (1/60 Hz) for power saving */
    ASSERT_EQ_HEX(mock_regs[0x0E], 0x03u);
}

TEST(test_timer_null)
{
    ASSERT_EQ(pcf8563_enable_timer(NULL, PCF8563_TIMER_1HZ, 10), PCF8563_ERR_PARAM);
    ASSERT_EQ(pcf8563_disable_timer(NULL), PCF8563_ERR_PARAM);
}

TEST(test_clkout_enable)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    ASSERT_EQ(pcf8563_set_clkout(&dev, true, PCF8563_CLKOUT_1HZ), PCF8563_OK);
    ASSERT_EQ(mock_regs[0x0D], 0x80 | 0x03); /* FE=1, FD=1Hz */
}

TEST(test_clkout_disable)
{
    mock_reset();
    pcf8563_init(&dev, &mock_i2c);

    pcf8563_set_clkout(&dev, true, PCF8563_CLKOUT_32768HZ);
    ASSERT_EQ(pcf8563_set_clkout(&dev, false, PCF8563_CLKOUT_32768HZ), PCF8563_OK);
    ASSERT_EQ(mock_regs[0x0D], 0x00);
}

TEST(test_clkout_null)
{
    ASSERT_EQ(pcf8563_set_clkout(NULL, true, PCF8563_CLKOUT_1HZ), PCF8563_ERR_PARAM);
}

/* Rev. 10 Table 27 */
TEST(test_mock_starts_at_the_reset_values)
{
    mock_reset();
    ASSERT_EQ_HEX(mock_regs[0x00], 0x08);         /* TESTC = 1 */
    ASSERT_EQ_HEX(mock_regs[0x01], 0x00);
    ASSERT_EQ_HEX(mock_regs[0x02] & 0x80u, 0x80); /* VL = 1 */
    for (int reg = 0x09; reg <= 0x0C; reg++)
        ASSERT_EQ_HEX(mock_regs[reg] & 0x80u, 0x80);  /* AE_x = 1 */
    ASSERT_EQ_HEX(mock_regs[0x0D] & 0x83u, 0x80); /* FE = 1, FD = 00 */
    ASSERT_EQ_HEX(mock_regs[0x0E] & 0x83u, 0x03); /* TE = 0, TD = 11 */
}

TEST_MAIN("pcf8563")
    RUN(test_init_ok);
    RUN(test_init_accepts_n_bits_high);
    RUN(test_init_refuses_test1_or_stop);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_read_time);
    RUN(test_read_time_null);
    RUN(test_read_time_i2c_fail);
    RUN(test_write_time);
    RUN(test_write_time_holds_stop_for_every_time_write);
    RUN(test_write_time_keeps_control_bits);
    RUN(test_write_time_failure_still_releases_stop);
    RUN(test_write_time_stop_write_fails);
    RUN(test_write_time_ctrl1_read_fails);
    RUN(test_write_time_release_fails);
    RUN(test_write_time_null);
    RUN(test_write_time_invalid);
    RUN(test_time_roundtrip);
    RUN(test_bcd_encoding_boundary);
    RUN(test_vl_bit_masked_on_read);
    RUN(test_alarm_write_read);
    RUN(test_alarm_all_disabled);
    RUN(test_alarm_null);
    RUN(test_write_alarm_invalid);
    RUN(test_timer_enable);
    RUN(test_timer_disable);
    RUN(test_timer_null);
    RUN(test_clkout_enable);
    RUN(test_clkout_disable);
    RUN(test_clkout_null);
    RUN(test_mock_starts_at_the_reset_values);
TEST_MAIN_END()
