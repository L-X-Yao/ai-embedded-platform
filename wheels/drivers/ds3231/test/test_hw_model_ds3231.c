/*
 * test_hw_model_ds3231.c -- tests using DS3231 hardware register model
 *
 * Validates DS3231 driver against a datasheet-accurate register model.
 * Tests BCD encoding/decoding, time set/get round-trip, temperature
 * readout, and 12/24-hour mode conversion during init.
 */

#include "test_harness.h"
#include "hw_model_ds3231.h"
#include "../ds3231.h"

static ds3231_model_t model;

static const ds3231_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static void setup(void)
{
    ds3231_model_init(&model);
}

/* --- init tests --- */

TEST(test_init_success)
{
    setup();
    ds3231_t dev;
    ds3231_err_t err = ds3231_init(&dev, &model_i2c);
    ASSERT_EQ(err, DS3231_OK);
}

TEST(test_init_forces_24h_mode)
{
    setup();
    /* Set 12-hour mode: 3:00 PM */
    ds3231_model_set_12h_mode(&model, 3, true);
    ASSERT_TRUE(model.hw.regs[DS3231_REG_HOURS] & 0x40);

    ds3231_t dev;
    ds3231_err_t err = ds3231_init(&dev, &model_i2c);
    ASSERT_EQ(err, DS3231_OK);

    /* Init should clear bit 6 and convert to 24-hour BCD for 15:00 */
    ASSERT_FALSE(model.hw.regs[DS3231_REG_HOURS] & 0x40);
    ASSERT_EQ(model.hw.regs[DS3231_REG_HOURS], ds3231_bin2bcd(15));
}

/* --- get_time tests --- */

TEST(test_get_time_reads_bcd)
{
    setup();
    ds3231_t dev;
    ds3231_init(&dev, &model_i2c);

    ds3231_time_t t;
    ds3231_err_t err = ds3231_get_time(&dev, &t);
    ASSERT_EQ(err, DS3231_OK);
    /* Default: 2025-01-15 Wed 12:30:45 */
    ASSERT_EQ(t.year, 25);
    ASSERT_EQ(t.month, 1);
    ASSERT_EQ(t.day, 15);
    ASSERT_EQ(t.weekday, 3);
    ASSERT_EQ(t.hour, 12);
    ASSERT_EQ(t.minute, 30);
    ASSERT_EQ(t.second, 45);
}

TEST(test_get_time_after_model_change)
{
    setup();
    ds3231_t dev;
    ds3231_init(&dev, &model_i2c);

    /* Simulate RTC ticking to 2026-06-20 Sat 08:15:00 */
    ds3231_model_set_time(&model, 26, 6, 20, 6, 8, 15, 0);

    ds3231_time_t t;
    ds3231_get_time(&dev, &t);
    ASSERT_EQ(t.year, 26);
    ASSERT_EQ(t.month, 6);
    ASSERT_EQ(t.day, 20);
    ASSERT_EQ(t.hour, 8);
    ASSERT_EQ(t.minute, 15);
    ASSERT_EQ(t.second, 0);
}

/* --- set_time tests --- */

TEST(test_set_time_round_trip)
{
    setup();
    ds3231_t dev;
    ds3231_init(&dev, &model_i2c);

    ds3231_time_t set_t = {
        .year = 26, .month = 12, .day = 31, .weekday = 4,
        .hour = 23, .minute = 59, .second = 58,
    };
    ds3231_err_t err = ds3231_set_time(&dev, &set_t);
    ASSERT_EQ(err, DS3231_OK);

    /* Verify BCD in model registers */
    ASSERT_EQ(model.hw.regs[DS3231_REG_YEAR], ds3231_bin2bcd(26));
    ASSERT_EQ(model.hw.regs[DS3231_REG_HOURS], ds3231_bin2bcd(23));

    /* Read back */
    ds3231_time_t got;
    ds3231_get_time(&dev, &got);
    ASSERT_EQ(got.year, 26);
    ASSERT_EQ(got.month, 12);
    ASSERT_EQ(got.day, 31);
    ASSERT_EQ(got.hour, 23);
    ASSERT_EQ(got.minute, 59);
    ASSERT_EQ(got.second, 58);
}

TEST(test_set_time_rejects_invalid)
{
    setup();
    ds3231_t dev;
    ds3231_init(&dev, &model_i2c);

    /* Invalid month=13 */
    ds3231_time_t bad = {
        .year = 25, .month = 13, .day = 1, .weekday = 1,
        .hour = 0, .minute = 0, .second = 0,
    };
    ASSERT_EQ(ds3231_set_time(&dev, &bad), DS3231_ERR_PARAM);
}

/* --- temperature tests --- */

TEST(test_temperature_read)
{
    setup();
    ds3231_t dev;
    ds3231_init(&dev, &model_i2c);

    /* Default: 23.25 C */
    float temp = 0.0f;
    ds3231_err_t err = ds3231_get_temperature(&dev, &temp);
    ASSERT_EQ(err, DS3231_OK);
    ASSERT_NEAR(temp, 23.25f, 0.01f);
}

TEST(test_temperature_negative)
{
    setup();
    ds3231_t dev;
    ds3231_init(&dev, &model_i2c);

    /* Set -10.75 C: MSB = -11 (0xF5 as signed), frac = 01 quarter = 0.25 */
    /* Actually: -10.75 = -11 + 0.25, so MSB = 0xF5 (-11), frac = 1 (0.25) */
    /* Wait, DS3231 temp encoding: MSB is signed integer, LSB upper 2 bits.
       For -10.75: MSB = 0xF5 (-11 signed), fraction = 0b01 (0.25)
       But that gives -11 + 0.25 = -10.75. Correct. */
    ds3231_model_set_temperature(&model, -11, 1);

    float temp = 0.0f;
    ds3231_get_temperature(&dev, &temp);
    ASSERT_NEAR(temp, -10.75f, 0.01f);
}

TEST(test_temperature_zero)
{
    setup();
    ds3231_t dev;
    ds3231_init(&dev, &model_i2c);

    ds3231_model_set_temperature(&model, 0, 0);
    float temp = -1.0f;
    ds3231_get_temperature(&dev, &temp);
    ASSERT_NEAR(temp, 0.0f, 0.01f);
}

/* --- register behavior tests --- */

TEST(test_temp_registers_readonly)
{
    setup();
    uint8_t original = model.hw.regs[DS3231_REG_TEMP_MSB];
    hw_model__write_reg(&model.hw, DS3231_REG_TEMP_MSB, 0xFF);
    ASSERT_EQ(model.hw.regs[DS3231_REG_TEMP_MSB], original);
}

TEST_MAIN("ds3231 hardware model")
    RUN(test_init_success);
    RUN(test_init_forces_24h_mode);
    RUN(test_get_time_reads_bcd);
    RUN(test_get_time_after_model_change);
    RUN(test_set_time_round_trip);
    RUN(test_set_time_rejects_invalid);
    RUN(test_temperature_read);
    RUN(test_temperature_negative);
    RUN(test_temperature_zero);
    RUN(test_temp_registers_readonly);
TEST_MAIN_END()
