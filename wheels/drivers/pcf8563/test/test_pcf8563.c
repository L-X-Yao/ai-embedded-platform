#include "test_harness.h"
#include "mock_i2c.h"
#include "pcf8563.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    /* PCF8563 POR: control registers read 0x00 (upper bits clear) */
    mock_regs[0x00] = 0x00;   /* Control_status_1 */
    mock_regs[0x01] = 0x00;   /* Control_status_2 */
    /* Default time: 2026-01-15 Thu 12:30:45 in BCD */
    mock_regs[0x02] = 0x45;   /* seconds: 45 = 0x45 BCD */
    mock_regs[0x03] = 0x30;   /* minutes: 30 = 0x30 BCD */
    mock_regs[0x04] = 0x12;   /* hours:   12 = 0x12 BCD */
    mock_regs[0x05] = 0x15;   /* days:    15 = 0x15 BCD */
    mock_regs[0x06] = 0x04;   /* weekday: 4 = Thursday */
    mock_regs[0x07] = 0x01;   /* month:   01 = 0x01 BCD */
    mock_regs[0x08] = 0x26;   /* year:    26 = 0x26 BCD */
    /* Alarms disabled by default */
    mock_regs[0x09] = 0x80;
    mock_regs[0x0A] = 0x80;
    mock_regs[0x0B] = 0x80;
    mock_regs[0x0C] = 0x80;
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
    ASSERT_EQ(mock_regs[0x0E], 0x00);
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

TEST_MAIN("pcf8563")
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_read_time);
    RUN(test_read_time_null);
    RUN(test_read_time_i2c_fail);
    RUN(test_write_time);
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
TEST_MAIN_END()
