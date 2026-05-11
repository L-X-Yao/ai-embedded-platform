// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "bno055.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = 0xA0;   /* CHIP_ID */
}

static const bno055_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static bno055_t dev;
static bno055_cfg_t default_cfg = {
    .addr = BNO055_DEV_ADDR,
    .mode = BNO055_MODE_NDOF,
};

static void setup(void)
{
    mock_reset();
    bno055_init(&dev, &mock_i2c, &default_cfg);
}

/* Plant Euler angles (int16 LE, 1/16 degree per LSB) */
static void plant_euler(int16_t heading, int16_t roll, int16_t pitch)
{
    mock_regs[0x1A] = (uint8_t)(heading & 0xFF);
    mock_regs[0x1B] = (uint8_t)((heading >> 8) & 0xFF);
    mock_regs[0x1C] = (uint8_t)(roll & 0xFF);
    mock_regs[0x1D] = (uint8_t)((roll >> 8) & 0xFF);
    mock_regs[0x1E] = (uint8_t)(pitch & 0xFF);
    mock_regs[0x1F] = (uint8_t)((pitch >> 8) & 0xFF);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    bno055_t d;
    bno055_err_t e = bno055_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BNO055_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x00] = 0x00;
    bno055_t d;
    bno055_err_t e = bno055_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BNO055_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    bno055_t d;
    bno055_err_t e = bno055_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BNO055_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(bno055_init(NULL, &mock_i2c, &default_cfg), BNO055_ERR_PARAM);
    bno055_t d;
    ASSERT_EQ(bno055_init(&d, NULL, &default_cfg), BNO055_ERR_PARAM);
    ASSERT_EQ(bno055_init(&d, &mock_i2c, NULL), BNO055_ERR_PARAM);
    bno055_euler_t euler;
    ASSERT_EQ(bno055_read_euler(NULL, &euler), BNO055_ERR_PARAM);
    ASSERT_EQ(bno055_read_euler(&d, NULL), BNO055_ERR_PARAM);
    bno055_calib_status_t calib;
    ASSERT_EQ(bno055_read_calib_status(NULL, &calib), BNO055_ERR_PARAM);
    ASSERT_EQ(bno055_read_calib_status(&d, NULL), BNO055_ERR_PARAM);
    ASSERT_EQ(bno055_set_mode(NULL, BNO055_MODE_NDOF), BNO055_ERR_PARAM);
}

TEST(test_zero_init_safety)
{
    bno055_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    bno055_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(bno055_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)BNO055_ERR_I2C);
    ASSERT_EQ(-2, (int)BNO055_ERR_ID);
    ASSERT_EQ(-3, (int)BNO055_ERR_PARAM);
}

TEST(test_init_sets_mode)
{
    mock_reset();
    bno055_t d;
    bno055_init(&d, &mock_i2c, &default_cfg);
    /* OPR_MODE should be set to NDOF (0x0C) */
    ASSERT_EQ(mock_regs[0x3D], 0x0C);
}

TEST(test_read_euler_north)
{
    setup();
    /* heading=0, roll=0, pitch=0 */
    plant_euler(0, 0, 0);
    bno055_euler_t euler;
    bno055_err_t e = bno055_read_euler(&dev, &euler);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_NEAR(euler.heading_deg, 0.0f, 0.01f);
    ASSERT_NEAR(euler.roll_deg, 0.0f, 0.01f);
    ASSERT_NEAR(euler.pitch_deg, 0.0f, 0.01f);
}

TEST(test_read_euler_90_heading)
{
    setup();
    /* 90 degrees = 90 * 16 = 1440 LSB */
    plant_euler(1440, 0, 0);
    bno055_euler_t euler;
    bno055_read_euler(&dev, &euler);
    ASSERT_NEAR(euler.heading_deg, 90.0f, 0.1f);
}

TEST(test_read_euler_negative_roll)
{
    setup();
    /* -45 degrees = -45 * 16 = -720 LSB */
    plant_euler(0, -720, 0);
    bno055_euler_t euler;
    bno055_read_euler(&dev, &euler);
    ASSERT_NEAR(euler.roll_deg, -45.0f, 0.1f);
}

TEST(test_read_euler_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bno055_euler_t euler;
    bno055_err_t e = bno055_read_euler(&dev, &euler);
    ASSERT_EQ(e, BNO055_ERR_I2C);
}

TEST(test_read_calib_status)
{
    setup();
    /* CALIB_STAT: sys=3, gyro=3, accel=2, mag=1 = 0b11_11_10_01 = 0xF9 */
    mock_regs[0x35] = 0xF9;
    bno055_calib_status_t calib;
    bno055_err_t e = bno055_read_calib_status(&dev, &calib);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_EQ(calib.sys, 3);
    ASSERT_EQ(calib.gyro, 3);
    ASSERT_EQ(calib.accel, 2);
    ASSERT_EQ(calib.mag, 1);
}

TEST(test_set_mode)
{
    setup();
    bno055_err_t e = bno055_set_mode(&dev, BNO055_MODE_IMU);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_EQ(mock_regs[0x3D], 0x08);
}

TEST(test_set_mode_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bno055_err_t e = bno055_set_mode(&dev, BNO055_MODE_CONFIG);
    ASSERT_EQ(e, BNO055_ERR_I2C);
}

TEST(test_delay_called)
{
    mock_reset();
    mock_delay_ms_count = 0;
    bno055_t d;
    bno055_init(&d, &mock_i2c, &default_cfg);
    /* Multiple delays: config mode, reset, re-check, operation mode */
    ASSERT_TRUE(mock_delay_ms_count >= 3);
}

TEST_MAIN("bno055")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_init_sets_mode);
    RUN(test_read_euler_north);
    RUN(test_read_euler_90_heading);
    RUN(test_read_euler_negative_roll);
    RUN(test_read_euler_i2c_fail);
    RUN(test_read_calib_status);
    RUN(test_set_mode);
    RUN(test_set_mode_i2c_fail);
    RUN(test_delay_called);
TEST_MAIN_END()
