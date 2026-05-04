/*
 * test_hw_model_tmp102.c — tests using hardware register model
 *
 * Validates TMP102 driver against a register model.
 * The model enforces temperature register read-only, config register
 * read-write, alert threshold registers, and shutdown/wake behavior.
 */

#include "test_harness.h"
#include "hw_model_tmp102.h"
#include "../tmp102.h"
#include <math.h>

static tmp102_model_t model;

static const tmp102_i2c_t model_i2c = {
    .write_reg  = tmp102_model_write_reg,
    .write_regs = tmp102_model_write_regs,
    .read_regs  = tmp102_model_read_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static const tmp102_cfg_t default_cfg = {
    .addr = TMP102_ADDR_GND,
};

static void setup(void)
{
    tmp102_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    tmp102_t dev;
    tmp102_err_t err = tmp102_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, TMP102_OK);
}

TEST(test_init_null_param)
{
    setup();
    tmp102_t dev;
    tmp102_err_t err = tmp102_init(&dev, NULL, &default_cfg);
    ASSERT_EQ(err, TMP102_ERR_PARAM);
}

TEST(test_init_clears_shutdown_bit)
{
    setup();
    /* Set SD bit (bit 0 of config byte 0) before init */
    uint8_t cfg_slot = TMP_SLOT(TMP102_REG_CONFIG);
    model.hw.regs[cfg_slot] |= 0x01;

    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    /* After init, SD bit should be cleared */
    ASSERT_EQ(model.hw.regs[cfg_slot] & 0x01, 0);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_25c)
{
    setup();
    tmp102_model_set_temperature(&model, 25.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    float temp;
    tmp102_err_t err = tmp102_read_temp(&dev, &temp);
    ASSERT_EQ(err, TMP102_OK);
    ASSERT_NEAR(temp, 25.0f, 0.1f);
}

TEST(test_read_negative)
{
    setup();
    tmp102_model_set_temperature(&model, -10.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    float temp;
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, -10.0f, 0.1f);
}

TEST(test_read_zero)
{
    setup();
    tmp102_model_set_temperature(&model, 0.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    float temp;
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, 0.0f, 0.1f);
}

TEST(test_read_high_temp)
{
    setup();
    tmp102_model_set_temperature(&model, 100.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    float temp;
    tmp102_read_temp(&dev, &temp);
    ASSERT_NEAR(temp, 100.0f, 0.2f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_temp_register_readonly)
{
    setup();
    tmp102_model_set_temperature(&model, 25.0f);
    tmp102_model_update_temp(&model);

    uint8_t slot = TMP_SLOT(TMP102_REG_TEMP);
    uint8_t original = model.hw.regs[slot];
    hw_model__write_reg(&model.hw, slot, 0xFF);
    ASSERT_EQ(model.hw.regs[slot], original);
}

TEST(test_shutdown_sets_sd_bit)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    tmp102_shutdown(&dev);

    uint8_t cfg_slot = TMP_SLOT(TMP102_REG_CONFIG);
    ASSERT_EQ(model.hw.regs[cfg_slot] & 0x01, 0x01);
}

TEST(test_wake_clears_sd_bit)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    tmp102_shutdown(&dev);
    tmp102_wake(&dev);

    uint8_t cfg_slot = TMP_SLOT(TMP102_REG_CONFIG);
    ASSERT_EQ(model.hw.regs[cfg_slot] & 0x01, 0x00);
}

TEST(test_set_alert_thresholds)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    /* Set alert high to 50 C */
    tmp102_set_alert_high(&dev, 50.0f);
    uint8_t thi_slot = TMP_SLOT(TMP102_REG_THIGH);
    /* 50 C: raw = 800, << 4 = 12800 = 0x3200 */
    ASSERT_EQ(model.hw.regs[thi_slot], 0x32);
    ASSERT_EQ(model.hw.regs[thi_slot + 1], 0x00);
}

TEST(test_different_temperatures)
{
    setup();
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    tmp102_model_set_temperature(&model, 10.0f);
    float t1;
    tmp102_read_temp(&dev, &t1);

    tmp102_model_set_temperature(&model, 40.0f);
    float t2;
    tmp102_read_temp(&dev, &t2);

    ASSERT_TRUE(t2 > t1);
}

TEST(test_multiple_reads_consistent)
{
    setup();
    tmp102_model_set_temperature(&model, 25.0f);
    tmp102_t dev;
    tmp102_init(&dev, &model_i2c, &default_cfg);

    float t1, t2;
    tmp102_read_temp(&dev, &t1);
    tmp102_read_temp(&dev, &t2);
    ASSERT_NEAR(t1, t2, 0.01f);
}

TEST_MAIN("tmp102 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_init_clears_shutdown_bit);
    RUN(test_read_25c);
    RUN(test_read_negative);
    RUN(test_read_zero);
    RUN(test_read_high_temp);
    RUN(test_temp_register_readonly);
    RUN(test_shutdown_sets_sd_bit);
    RUN(test_wake_clears_sd_bit);
    RUN(test_set_alert_thresholds);
    RUN(test_different_temperatures);
    RUN(test_multiple_reads_consistent);
TEST_MAIN_END()
