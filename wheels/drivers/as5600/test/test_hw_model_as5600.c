// SPDX-License-Identifier: MIT
/*
 * test_hw_model_as5600.c — tests using hardware register model
 *
 * Validates AS5600 driver against a datasheet-accurate register model.
 * The model enforces read-only angle/status registers, 12-bit angle
 * output, and writable ZPOS/MANG configuration.
 */

#include "test_harness.h"
#include "hw_model_as5600.h"
#include "../as5600.h"
#include <math.h>

static as5600_model_t model;

static const as5600_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static void setup(void)
{
    as5600_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    as5600_t dev;
    as5600_err_t err = as5600_init(&dev, &model_i2c);
    ASSERT_EQ(err, AS5600_OK);
}

TEST(test_init_null_param)
{
    setup();
    as5600_t dev;
    as5600_err_t err = as5600_init(&dev, NULL);
    ASSERT_EQ(err, AS5600_ERR_PARAM);
}

/* --- angle read tests -------------------------------------------------- */

TEST(test_read_raw_angle_180)
{
    setup();
    /* Default angle is 2048 = 180 degrees */
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    uint16_t raw;
    as5600_err_t err = as5600_read_raw_angle(&dev, &raw);
    ASSERT_EQ(err, AS5600_OK);
    ASSERT_EQ(raw, 2048);
}

TEST(test_read_raw_angle_zero)
{
    setup();
    as5600_model_set_angle(&model, 0);
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    uint16_t raw;
    as5600_read_raw_angle(&dev, &raw);
    ASSERT_EQ(raw, 0);
}

TEST(test_read_raw_angle_max)
{
    setup();
    as5600_model_set_angle(&model, 4095);
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    uint16_t raw;
    as5600_read_raw_angle(&dev, &raw);
    ASSERT_EQ(raw, 4095);
}

TEST(test_read_degrees_180)
{
    setup();
    as5600_model_set_angle(&model, 2048);
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    float degrees;
    as5600_err_t err = as5600_read_degrees(&dev, &degrees);
    ASSERT_EQ(err, AS5600_OK);
    ASSERT_NEAR(degrees, 180.0f, 0.1f);
}

TEST(test_read_degrees_zero)
{
    setup();
    as5600_model_set_angle(&model, 0);
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    float degrees;
    as5600_read_degrees(&dev, &degrees);
    ASSERT_NEAR(degrees, 0.0f, 0.1f);
}

TEST(test_read_degrees_near_360)
{
    setup();
    as5600_model_set_angle(&model, 4095);
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    float degrees;
    as5600_read_degrees(&dev, &degrees);
    ASSERT_TRUE(degrees > 359.0f && degrees < 360.0f);
}

/* --- status tests ------------------------------------------------------ */

TEST(test_read_status_magnet_detected)
{
    setup();
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    uint8_t status;
    as5600_err_t err = as5600_read_status(&dev, &status);
    ASSERT_EQ(err, AS5600_OK);
    ASSERT_TRUE(status & AS5600_STATUS_MD);
}

TEST(test_read_status_no_magnet)
{
    setup();
    as5600_model_set_status(&model, 0);
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    uint8_t status;
    as5600_read_status(&dev, &status);
    ASSERT_FALSE(status & AS5600_STATUS_MD);
}

TEST(test_read_status_magnet_too_strong)
{
    setup();
    as5600_model_set_status(&model, AS5600_STATUS_MD | AS5600_STATUS_MH);
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    uint8_t status;
    as5600_read_status(&dev, &status);
    ASSERT_TRUE(status & AS5600_STATUS_MH);
    ASSERT_TRUE(status & AS5600_STATUS_MD);
}

/* --- config tests ------------------------------------------------------ */

TEST(test_set_start_position)
{
    setup();
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    as5600_err_t err = as5600_set_start_position(&dev, 1024);
    ASSERT_EQ(err, AS5600_OK);

    /* Verify ZPOS register updated */
    uint16_t zpos = (uint16_t)(((uint16_t)(model.hw.regs[AS5600_REG_ZPOS_H] & 0x0Fu) << 8)
                  | model.hw.regs[AS5600_REG_ZPOS_L]);
    ASSERT_EQ(zpos, 1024);
    /* Blank bits keep their factory setting */
    ASSERT_EQ(model.hw.regs[AS5600_REG_ZPOS_H] & 0xF0u, AS5600_MODEL_BLANK_BITS);
}

TEST(test_set_max_angle)
{
    setup();
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    as5600_err_t err = as5600_set_max_angle(&dev, 2048);
    ASSERT_EQ(err, AS5600_OK);

    uint16_t mang = (uint16_t)(((uint16_t)(model.hw.regs[AS5600_REG_MANG_H] & 0x0Fu) << 8)
                  | model.hw.regs[AS5600_REG_MANG_L]);
    ASSERT_EQ(mang, 2048);
    ASSERT_EQ(model.hw.regs[AS5600_REG_MANG_H] & 0xF0u, AS5600_MODEL_BLANK_BITS);
}

TEST(test_set_start_position_out_of_range)
{
    setup();
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    as5600_err_t err = as5600_set_start_position(&dev, 5000);
    ASSERT_EQ(err, AS5600_ERR_PARAM);
}

/* --- datasheet behavior tests ----------------------------------------- */

TEST(test_raw_angle_readonly)
{
    setup();
    as5600_model_set_angle(&model, 1000);
    uint8_t original_h = model.hw.regs[AS5600_REG_RAW_ANG_H];
    hw_model__write_reg(&model.hw, AS5600_REG_RAW_ANG_H, 0xFF);
    ASSERT_EQ(model.hw.regs[AS5600_REG_RAW_ANG_H], original_h);
}

TEST(test_status_readonly)
{
    setup();
    uint8_t original = model.hw.regs[AS5600_REG_STATUS];
    hw_model__write_reg(&model.hw, AS5600_REG_STATUS, 0xFF);
    ASSERT_EQ(model.hw.regs[AS5600_REG_STATUS], original);
}

TEST(test_varying_angles)
{
    setup();
    as5600_t dev;
    as5600_init(&dev, &model_i2c);

    as5600_model_set_angle(&model, 500);
    float d1;
    as5600_read_degrees(&dev, &d1);

    as5600_model_set_angle(&model, 3000);
    float d2;
    as5600_read_degrees(&dev, &d2);

    ASSERT_TRUE(d2 > d1);
}

TEST_MAIN("as5600 hardware model")
    RUN(test_init_ok);
    RUN(test_init_null_param);
    RUN(test_read_raw_angle_180);
    RUN(test_read_raw_angle_zero);
    RUN(test_read_raw_angle_max);
    RUN(test_read_degrees_180);
    RUN(test_read_degrees_zero);
    RUN(test_read_degrees_near_360);
    RUN(test_read_status_magnet_detected);
    RUN(test_read_status_no_magnet);
    RUN(test_read_status_magnet_too_strong);
    RUN(test_set_start_position);
    RUN(test_set_max_angle);
    RUN(test_set_start_position_out_of_range);
    RUN(test_raw_angle_readonly);
    RUN(test_status_readonly);
    RUN(test_varying_angles);
TEST_MAIN_END()
