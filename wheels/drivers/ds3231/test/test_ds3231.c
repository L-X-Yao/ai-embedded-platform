// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "ds3231.h"

/* ---------- chip-specific mock setup ---------- */

/*
 * DS3231 register layout (defaults = midnight 2000-01-01, Mon, 24h mode):
 *   0x00 seconds (BCD)    = 0x00
 *   0x01 minutes (BCD)    = 0x00
 *   0x02 hours   (BCD 24h)= 0x00  (bit6=0 → 24h)
 *   0x03 day-of-week      = 0x01  (Monday)
 *   0x04 date    (BCD)    = 0x01
 *   0x05 month   (BCD)    = 0x01
 *   0x06 year    (BCD)    = 0x00  (2000)
 *   0x11 temp MSB         = 0x19  (25)
 *   0x12 temp LSB         = 0xC0  (0.75 fraction → 25.75C)
 */
static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = 0x00;
    mock_regs[0x01] = 0x00;
    mock_regs[0x02] = 0x00;
    mock_regs[0x03] = 0x01;
    mock_regs[0x04] = 0x01;
    mock_regs[0x05] = 0x01;
    mock_regs[0x06] = 0x00;
    mock_regs[0x11] = 0x19;  /* 25 */
    mock_regs[0x12] = 0xC0;  /* .75 */
}

static const ds3231_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static ds3231_t dev;

/* ---------- helpers ---------- */

static void plant_time_bcd(uint8_t sec, uint8_t min, uint8_t hr,
                           uint8_t wday, uint8_t date, uint8_t mon,
                           uint8_t yr)
{
    mock_regs[0x00] = sec;
    mock_regs[0x01] = min;
    mock_regs[0x02] = hr;
    mock_regs[0x03] = wday;
    mock_regs[0x04] = date;
    mock_regs[0x05] = mon;
    mock_regs[0x06] = yr;
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    ASSERT_EQ(ds3231_init(&dev, &mock_i2c), DS3231_OK);
}

TEST(test_init_null_dev)
{
    mock_reset();
    ASSERT_EQ(ds3231_init(NULL, &mock_i2c), DS3231_ERR_PARAM);
}

TEST(test_init_null_i2c)
{
    mock_reset();
    ASSERT_EQ(ds3231_init(&dev, NULL), DS3231_ERR_PARAM);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    ASSERT_EQ(ds3231_init(&dev, &mock_i2c), DS3231_ERR_I2C);
}

TEST(test_get_time_known_bcd)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    /* Plant 2025-12-31 Wed 23:59:45 in BCD */
    plant_time_bcd(0x45, 0x59, 0x23, 0x03, 0x31, 0x12, 0x25);

    ds3231_time_t t;
    ASSERT_EQ(ds3231_get_time(&dev, &t), DS3231_OK);
    ASSERT_EQ(t.second,  45);
    ASSERT_EQ(t.minute,  59);
    ASSERT_EQ(t.hour,    23);
    ASSERT_EQ(t.weekday, 3);
    ASSERT_EQ(t.day,     31);
    ASSERT_EQ(t.month,   12);
    ASSERT_EQ(t.year,    25);
}

TEST(test_set_time_bcd_encoding)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t = {
        .year = 25, .month = 12, .day = 31,
        .weekday = 3, .hour = 23, .minute = 59, .second = 45
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t), DS3231_OK);

    ASSERT_EQ(mock_regs[0x00], 0x45);  /* 45 → BCD 0x45 */
    ASSERT_EQ(mock_regs[0x01], 0x59);  /* 59 → BCD 0x59 */
    ASSERT_EQ(mock_regs[0x02], 0x23);  /* 23 → BCD 0x23 */
    ASSERT_EQ(mock_regs[0x03], 3);     /* weekday not BCD */
    ASSERT_EQ(mock_regs[0x04], 0x31);  /* 31 → BCD 0x31 */
    ASSERT_EQ(mock_regs[0x05], 0x12);  /* 12 → BCD 0x12 */
    ASSERT_EQ(mock_regs[0x06], 0x25);  /* 25 → BCD 0x25 */
}

TEST(test_bcd_roundtrip_0)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t_in = {
        .year = 0, .month = 1, .day = 1,
        .weekday = 1, .hour = 0, .minute = 0, .second = 0
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t_in), DS3231_OK);

    ds3231_time_t t_out;
    ASSERT_EQ(ds3231_get_time(&dev, &t_out), DS3231_OK);
    ASSERT_EQ(t_out.year,    0);
    ASSERT_EQ(t_out.month,   1);
    ASSERT_EQ(t_out.day,     1);
    ASSERT_EQ(t_out.hour,    0);
    ASSERT_EQ(t_out.minute,  0);
    ASSERT_EQ(t_out.second,  0);
}

TEST(test_bcd_roundtrip_59)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t_in = {
        .year = 99, .month = 12, .day = 31,
        .weekday = 7, .hour = 23, .minute = 59, .second = 59
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t_in), DS3231_OK);

    ds3231_time_t t_out;
    ASSERT_EQ(ds3231_get_time(&dev, &t_out), DS3231_OK);
    ASSERT_EQ(t_out.year,    99);
    ASSERT_EQ(t_out.month,   12);
    ASSERT_EQ(t_out.day,     31);
    ASSERT_EQ(t_out.hour,    23);
    ASSERT_EQ(t_out.minute,  59);
    ASSERT_EQ(t_out.second,  59);
    ASSERT_EQ(t_out.weekday, 7);
}

TEST(test_bcd_roundtrip_10)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t_in = {
        .year = 10, .month = 10, .day = 10,
        .weekday = 1, .hour = 10, .minute = 10, .second = 10
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t_in), DS3231_OK);

    ds3231_time_t t_out;
    ASSERT_EQ(ds3231_get_time(&dev, &t_out), DS3231_OK);
    ASSERT_EQ(t_out.year,   10);
    ASSERT_EQ(t_out.hour,   10);
    ASSERT_EQ(t_out.minute, 10);
    ASSERT_EQ(t_out.second, 10);
}

TEST(test_bcd_roundtrip_9)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t_in = {
        .year = 9, .month = 9, .day = 9,
        .weekday = 1, .hour = 9, .minute = 9, .second = 9
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t_in), DS3231_OK);

    ds3231_time_t t_out;
    ASSERT_EQ(ds3231_get_time(&dev, &t_out), DS3231_OK);
    ASSERT_EQ(t_out.year,   9);
    ASSERT_EQ(t_out.hour,   9);
    ASSERT_EQ(t_out.minute, 9);
    ASSERT_EQ(t_out.second, 9);
}

TEST(test_temp_positive)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    /* 25.75C: MSB=0x19 (25), LSB=0xC0 (frac=3 → 0.75) */
    mock_regs[0x11] = 0x19;
    mock_regs[0x12] = 0xC0;

    float t;
    ASSERT_EQ(ds3231_get_temperature(&dev, &t), DS3231_OK);
    ASSERT_NEAR(t, 25.75f, 0.01f);
}

TEST(test_temp_zero)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    mock_regs[0x11] = 0x00;
    mock_regs[0x12] = 0x00;

    float t;
    ASSERT_EQ(ds3231_get_temperature(&dev, &t), DS3231_OK);
    ASSERT_NEAR(t, 0.0f, 0.01f);
}

TEST(test_temp_negative)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    /* -10.25C: MSB=0xF6 (-10 as int8), LSB=0x40 (frac=1 → 0.25) */
    mock_regs[0x11] = 0xF6;  /* -10 in two's complement */
    mock_regs[0x12] = 0x40;

    float t;
    ASSERT_EQ(ds3231_get_temperature(&dev, &t), DS3231_OK);
    ASSERT_NEAR(t, -9.75f, 0.01f);
}

TEST(test_midnight)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t_in = {
        .year = 0, .month = 1, .day = 1,
        .weekday = 1, .hour = 0, .minute = 0, .second = 0
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t_in), DS3231_OK);

    ds3231_time_t t_out;
    ASSERT_EQ(ds3231_get_time(&dev, &t_out), DS3231_OK);
    ASSERT_EQ(t_out.hour,   0);
    ASSERT_EQ(t_out.minute, 0);
    ASSERT_EQ(t_out.second, 0);
}

TEST(test_end_of_month)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t_in = {
        .year = 25, .month = 1, .day = 31,
        .weekday = 5, .hour = 23, .minute = 59, .second = 59
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t_in), DS3231_OK);

    ds3231_time_t t_out;
    ASSERT_EQ(ds3231_get_time(&dev, &t_out), DS3231_OK);
    ASSERT_EQ(t_out.day, 31);
}

TEST(test_i2c_fail_get_time)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);
    mock_fail_next = 1;

    ds3231_time_t t;
    ASSERT_EQ(ds3231_get_time(&dev, &t), DS3231_ERR_I2C);
}

TEST(test_i2c_fail_set_time)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);
    mock_fail_next = 1;

    ds3231_time_t t = {
        .year = 25, .month = 6, .day = 15,
        .weekday = 1, .hour = 12, .minute = 30, .second = 0
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t), DS3231_ERR_I2C);
}

TEST(test_i2c_fail_get_temp)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);
    mock_fail_next = 1;

    float t;
    ASSERT_EQ(ds3231_get_temperature(&dev, &t), DS3231_ERR_I2C);
}

TEST(test_invalid_month_zero)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t = {
        .year = 25, .month = 0, .day = 15,
        .weekday = 1, .hour = 12, .minute = 30, .second = 0
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t), DS3231_ERR_PARAM);
}

TEST(test_invalid_month_13)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t = {
        .year = 25, .month = 13, .day = 15,
        .weekday = 1, .hour = 12, .minute = 30, .second = 0
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t), DS3231_ERR_PARAM);
}

TEST(test_invalid_hour_24)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t = {
        .year = 25, .month = 6, .day = 15,
        .weekday = 1, .hour = 24, .minute = 30, .second = 0
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t), DS3231_ERR_PARAM);
}

TEST(test_invalid_second_60)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t = {
        .year = 25, .month = 6, .day = 15,
        .weekday = 1, .hour = 12, .minute = 30, .second = 60
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t), DS3231_ERR_PARAM);
}

TEST(test_invalid_day_zero)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);

    ds3231_time_t t = {
        .year = 25, .month = 6, .day = 0,
        .weekday = 1, .hour = 12, .minute = 30, .second = 0
    };
    ASSERT_EQ(ds3231_set_time(&dev, &t), DS3231_ERR_PARAM);
}

TEST(test_get_time_null_dev)
{
    ds3231_time_t t;
    ASSERT_EQ(ds3231_get_time(NULL, &t), DS3231_ERR_PARAM);
}

TEST(test_get_time_null_out)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);
    ASSERT_EQ(ds3231_get_time(&dev, NULL), DS3231_ERR_PARAM);
}

TEST(test_set_time_null_dev)
{
    ds3231_time_t t = {
        .year = 25, .month = 6, .day = 15,
        .weekday = 1, .hour = 12, .minute = 0, .second = 0
    };
    ASSERT_EQ(ds3231_set_time(NULL, &t), DS3231_ERR_PARAM);
}

TEST(test_get_temp_null_dev)
{
    float t;
    ASSERT_EQ(ds3231_get_temperature(NULL, &t), DS3231_ERR_PARAM);
}

TEST(test_get_temp_null_out)
{
    mock_reset();
    ds3231_init(&dev, &mock_i2c);
    ASSERT_EQ(ds3231_get_temperature(&dev, NULL), DS3231_ERR_PARAM);
}

TEST_MAIN("ds3231")
    RUN(test_init_ok);
    RUN(test_init_null_dev);
    RUN(test_init_null_i2c);
    RUN(test_init_i2c_fail);
    RUN(test_get_time_known_bcd);
    RUN(test_set_time_bcd_encoding);
    RUN(test_bcd_roundtrip_0);
    RUN(test_bcd_roundtrip_59);
    RUN(test_bcd_roundtrip_10);
    RUN(test_bcd_roundtrip_9);
    RUN(test_temp_positive);
    RUN(test_temp_zero);
    RUN(test_temp_negative);
    RUN(test_midnight);
    RUN(test_end_of_month);
    RUN(test_i2c_fail_get_time);
    RUN(test_i2c_fail_set_time);
    RUN(test_i2c_fail_get_temp);
    RUN(test_invalid_month_zero);
    RUN(test_invalid_month_13);
    RUN(test_invalid_hour_24);
    RUN(test_invalid_second_60);
    RUN(test_invalid_day_zero);
    RUN(test_get_time_null_dev);
    RUN(test_get_time_null_out);
    RUN(test_set_time_null_dev);
    RUN(test_get_temp_null_dev);
    RUN(test_get_temp_null_out);
TEST_MAIN_END()
