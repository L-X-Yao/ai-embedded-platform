// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "bno055.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_regs[0x00] = 0xA0;  /* CHIP_ID */
}

static const bno055_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static bno055_t dev;
static bno055_cfg_t default_cfg = {
    .addr = BNO055_ADDR_PRIMARY,
    .mode = BNO055_MODE_NDOF,
};

static void setup(void)
{
    mock_reset();
    bno055_init(&dev, &mock_i2c, &default_cfg);
}

/* Plant Euler angles: heading, roll, pitch as int16 LE at 0x1A-0x1F */
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
    bno055_calib_status_t cal;
    ASSERT_EQ(bno055_get_calib_status(NULL, &cal), BNO055_ERR_PARAM);
    ASSERT_EQ(bno055_get_calib_status(&d, NULL), BNO055_ERR_PARAM);
    ASSERT_EQ(bno055_set_mode(NULL, BNO055_MODE_NDOF), BNO055_ERR_PARAM);
}

TEST(test_read_euler_north)
{
    setup();
    /* 0 degrees heading, 0 roll, 0 pitch (all zeros) */
    plant_euler(0, 0, 0);
    bno055_euler_t euler;
    bno055_err_t e = bno055_read_euler(&dev, &euler);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_NEAR(euler.heading_deg, 0.0f, 0.01f);
    ASSERT_NEAR(euler.roll_deg, 0.0f, 0.01f);
    ASSERT_NEAR(euler.pitch_deg, 0.0f, 0.01f);
}

TEST(test_read_euler_90deg)
{
    setup();
    /* 90 degrees = 90 * 16 = 1440 LSB */
    plant_euler(1440, 0, 0);
    bno055_euler_t euler;
    bno055_err_t e = bno055_read_euler(&dev, &euler);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_NEAR(euler.heading_deg, 90.0f, 0.01f);
}

TEST(test_read_euler_negative)
{
    setup();
    /* -45 degrees = -45 * 16 = -720 LSB */
    plant_euler(0, -720, 0);
    bno055_euler_t euler;
    bno055_err_t e = bno055_read_euler(&dev, &euler);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_NEAR(euler.roll_deg, -45.0f, 0.01f);
}

TEST(test_read_euler_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bno055_euler_t euler;
    bno055_err_t e = bno055_read_euler(&dev, &euler);
    ASSERT_EQ(e, BNO055_ERR_I2C);
}

TEST(test_calib_status)
{
    setup();
    /* CALIB_STAT: sys=3 (bits7:6), gyro=2 (bits5:4), accel=1 (bits3:2), mag=0 (bits1:0) */
    mock_regs[0x35] = 0xE4;  /* 11 10 01 00 = 0xE4 */
    bno055_calib_status_t cal;
    bno055_err_t e = bno055_get_calib_status(&dev, &cal);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_EQ(cal.sys, 3);
    ASSERT_EQ(cal.gyro, 2);
    ASSERT_EQ(cal.accel, 1);
    ASSERT_EQ(cal.mag, 0);
}

TEST(test_calib_status_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bno055_calib_status_t cal;
    bno055_err_t e = bno055_get_calib_status(&dev, &cal);
    ASSERT_EQ(e, BNO055_ERR_I2C);
}

TEST(test_set_mode)
{
    setup();
    bno055_err_t e = bno055_set_mode(&dev, BNO055_MODE_IMU);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_EQ(mock_regs[0x3D], BNO055_MODE_IMU);
}

TEST(test_set_mode_invalid)
{
    setup();
    bno055_err_t e = bno055_set_mode(&dev, (bno055_opr_mode_t)0x0D);
    ASSERT_EQ(e, BNO055_ERR_PARAM);
}

TEST(test_set_mode_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bno055_err_t e = bno055_set_mode(&dev, BNO055_MODE_NDOF);
    ASSERT_EQ(e, BNO055_ERR_I2C);
}

TEST(test_init_mode_register)
{
    setup();
    /* After init with NDOF, OPR_MODE register should be 0x0C */
    ASSERT_EQ(mock_regs[0x3D], BNO055_MODE_NDOF);
}

TEST(test_addr_alt)
{
    mock_reset();
    bno055_cfg_t cfg = {
        .addr = BNO055_ADDR_ALT,
        .mode = BNO055_MODE_NDOF,
    };
    bno055_t d;
    bno055_err_t e = bno055_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BNO055_OK);
}

TEST(test_delay_called_for_fusion)
{
    mock_reset();
    uint32_t before = mock_delay_ms_count;
    bno055_t d;
    bno055_cfg_t cfg = {
        .addr = BNO055_ADDR_PRIMARY,
        .mode = BNO055_MODE_NDOF,
    };
    bno055_init(&d, &mock_i2c, &cfg);
    /* Should have delays: CONFIG switch (19ms) + to NDOF (7ms + 600ms) = 3 delay calls */
    ASSERT_TRUE(mock_delay_ms_count >= before + 2);
}

TEST(test_init_invalid_mode)
{
    mock_reset();
    bno055_cfg_t cfg = {
        .addr = BNO055_ADDR_PRIMARY,
        .mode = (bno055_opr_mode_t)0xFF,
    };
    bno055_t d;
    bno055_err_t e = bno055_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BNO055_ERR_PARAM);
}

TEST(test_read_euler_360)
{
    setup();
    /* 359.9375 degrees = 5759 * (1/16) */
    plant_euler(5759, 0, 0);
    bno055_euler_t euler;
    bno055_err_t e = bno055_read_euler(&dev, &euler);
    ASSERT_EQ(e, BNO055_OK);
    ASSERT_NEAR(euler.heading_deg, 359.9375f, 0.01f);
}

TEST(test_read_euler_needs_fusion_mode)
{
    bno055_euler_t euler;
    setup();

    /* CONFIG output is all zero, which would read as level and pointing north. */
    ASSERT_EQ(bno055_set_mode(&dev, BNO055_MODE_CONFIG), BNO055_OK);
    ASSERT_EQ(bno055_read_euler(&dev, &euler), BNO055_ERR_PARAM);

    ASSERT_EQ(bno055_set_mode(&dev, BNO055_MODE_NDOF), BNO055_OK);
    ASSERT_EQ(bno055_read_euler(&dev, &euler), BNO055_OK);
}

TEST(test_init_requires_delay)
{
    static const bno055_i2c_t no_delay = {
        .write_reg = mock_i2c_write_reg,
        .read_regs = mock_i2c_read_regs,
        .delay_ms  = NULL,
        .ctx       = NULL,
    };
    bno055_t d;
    mock_reset();

    ASSERT_EQ(bno055_init(&d, &no_delay, &default_cfg), BNO055_ERR_PARAM);
}

/* The chip stays in CONFIG, whose output is all zero, so reads must be refused */
TEST(test_read_euler_refused_after_failed_mode_change)
{
    bno055_euler_t euler;
    setup();

    mock_fail_at = mock_op_count + 1;  /* the write out of CONFIG */
    ASSERT_EQ(bno055_set_mode(&dev, BNO055_MODE_IMU), BNO055_ERR_I2C);
    ASSERT_EQ(mock_regs[0x3D], BNO055_MODE_CONFIG);
    ASSERT_EQ(dev.cfg.mode, BNO055_MODE_CONFIG);
    ASSERT_EQ(bno055_read_euler(&dev, &euler), BNO055_ERR_PARAM);

    ASSERT_EQ(bno055_set_mode(&dev, BNO055_MODE_IMU), BNO055_OK);
    ASSERT_EQ(bno055_read_euler(&dev, &euler), BNO055_OK);
}

/* When the write into CONFIG fails the chip has not moved at all */
TEST(test_read_euler_works_after_failed_write_into_config)
{
    bno055_euler_t euler;
    setup();

    mock_fail_next = 1;
    ASSERT_EQ(bno055_set_mode(&dev, BNO055_MODE_IMU), BNO055_ERR_I2C);
    ASSERT_EQ(mock_regs[0x3D], BNO055_MODE_NDOF);
    ASSERT_EQ(dev.cfg.mode, BNO055_MODE_NDOF);
    ASSERT_EQ(bno055_read_euler(&dev, &euler), BNO055_OK);
}

TEST(test_read_euler_refused_after_failed_init)
{
    bno055_euler_t euler;
    mock_reset();

    mock_fail_at = 2;  /* after the ID read and the write into CONFIG */
    ASSERT_EQ(bno055_init(&dev, &mock_i2c, &default_cfg), BNO055_ERR_I2C);
    ASSERT_EQ(mock_regs[0x3D], BNO055_MODE_CONFIG);
    ASSERT_EQ(bno055_read_euler(&dev, &euler), BNO055_ERR_PARAM);
}

TEST_MAIN("bno055")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_euler_north);
    RUN(test_read_euler_90deg);
    RUN(test_read_euler_negative);
    RUN(test_read_euler_i2c_fail);
    RUN(test_calib_status);
    RUN(test_calib_status_i2c_fail);
    RUN(test_set_mode);
    RUN(test_set_mode_invalid);
    RUN(test_set_mode_i2c_fail);
    RUN(test_init_mode_register);
    RUN(test_addr_alt);
    RUN(test_delay_called_for_fusion);
    RUN(test_init_invalid_mode);
    RUN(test_read_euler_360);
    RUN(test_read_euler_needs_fusion_mode);
    RUN(test_init_requires_delay);
    RUN(test_read_euler_refused_after_failed_mode_change);
    RUN(test_read_euler_works_after_failed_write_into_config);
    RUN(test_read_euler_refused_after_failed_init);
TEST_MAIN_END()
