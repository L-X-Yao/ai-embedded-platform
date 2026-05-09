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

TEST(test_hw_read_por_time)
{
    hw_model_reset();
    pcf8563_init(&dev, &hw_i2c);

    pcf8563_time_t t;
    ASSERT_EQ(pcf8563_read_time(&dev, &t), PCF8563_OK);
    /* POR defaults: 2026-03-20 Fri 14:30:00 */
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

TEST_MAIN("pcf8563_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_read_por_time);
    RUN(test_hw_write_read_roundtrip);
    RUN(test_hw_inject_time);
    RUN(test_hw_alarm_roundtrip);
TEST_MAIN_END()
