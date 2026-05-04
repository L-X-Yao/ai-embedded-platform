/*
 * test_hw_model_mpu6050.c — tests using hardware register model
 *
 * Validates MPU6050 driver against a datasheet-accurate register model.
 * The model enforces WHO_AM_I read-only, device reset via PWR_MGMT_1,
 * data register read-only, and accel/gyro scaling.
 */

#include "test_harness.h"
#include "hw_model_mpu6050.h"
#include "../mpu6050.h"
#include <math.h>

static mpu6050_model_t model;

static const mpu6050_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const mpu6050_cfg_t default_cfg = {
    .addr        = MPU6050_ADDR_LOW,
    .accel_range = MPU6050_ACCEL_2G,
    .gyro_range  = MPU6050_GYRO_250DPS,
};

static void setup(void)
{
    mpu6050_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_reads_who_am_i)
{
    setup();
    mpu6050_t dev;
    mpu6050_err_t err = mpu6050_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, MPU6050_OK);
}

TEST(test_init_wrong_who_am_i)
{
    setup();
    model.hw.reset_values[MPU6050_REG_WHO_AM_I] = 0xFF;
    hw_model_reset(&model.hw);
    mpu6050_model_set_data_regs(&model);

    mpu6050_t dev;
    mpu6050_err_t err = mpu6050_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, MPU6050_ERR_ID);
}

TEST(test_init_writes_accel_config)
{
    setup();
    mpu6050_t dev;
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_8G,
                          .gyro_range = MPU6050_GYRO_250DPS };
    mpu6050_init(&dev, &model_i2c, &cfg);
    /* ACCEL_CONFIG: AFS_SEL[4:3] = 2 for 8G */
    ASSERT_EQ(model.hw.regs[MPU6050_REG_ACCEL_CONFIG] & 0x18, 2 << 3);
}

TEST(test_init_writes_gyro_config)
{
    setup();
    mpu6050_t dev;
    mpu6050_cfg_t cfg = { .addr = MPU6050_ADDR_LOW,
                          .accel_range = MPU6050_ACCEL_2G,
                          .gyro_range = MPU6050_GYRO_1000DPS };
    mpu6050_init(&dev, &model_i2c, &cfg);
    /* GYRO_CONFIG: FS_SEL[4:3] = 2 for 1000DPS */
    ASSERT_EQ(model.hw.regs[MPU6050_REG_GYRO_CONFIG] & 0x18, 2 << 3);
}

TEST(test_init_resets_device)
{
    setup();
    mpu6050_t dev;
    int before = model.reset_count;
    mpu6050_init(&dev, &model_i2c, &default_cfg);
    /* init writes PWR_MGMT_1 with DEVICE_RESET bit */
    ASSERT_TRUE(model.reset_count > before);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_accel_1g_z)
{
    setup();
    mpu6050_t dev;
    mpu6050_init(&dev, &model_i2c, &default_cfg);
    /* Model default: az = 16384 raw, at 2G range (16384 LSB/g) -> 1.0g */
    mpu6050_model_set_accel(&model, 0, 0, 16384);

    mpu6050_accel_t accel;
    mpu6050_err_t err = mpu6050_read_accel(&dev, &accel);
    ASSERT_EQ(err, MPU6050_OK);
    ASSERT_NEAR(accel.ax_g, 0.0f, 0.01f);
    ASSERT_NEAR(accel.ay_g, 0.0f, 0.01f);
    ASSERT_NEAR(accel.az_g, 1.0f, 0.01f);
}

TEST(test_read_gyro_zero)
{
    setup();
    mpu6050_t dev;
    mpu6050_init(&dev, &model_i2c, &default_cfg);
    mpu6050_model_set_gyro(&model, 0, 0, 0);

    mpu6050_gyro_t gyro;
    mpu6050_err_t err = mpu6050_read_gyro(&dev, &gyro);
    ASSERT_EQ(err, MPU6050_OK);
    ASSERT_NEAR(gyro.gx_dps, 0.0f, 0.01f);
    ASSERT_NEAR(gyro.gy_dps, 0.0f, 0.01f);
    ASSERT_NEAR(gyro.gz_dps, 0.0f, 0.01f);
}

TEST(test_read_accel_negative)
{
    setup();
    mpu6050_t dev;
    mpu6050_init(&dev, &model_i2c, &default_cfg);
    /* -1g on X */
    mpu6050_model_set_accel(&model, -16384, 0, 0);

    mpu6050_accel_t accel;
    mpu6050_read_accel(&dev, &accel);
    ASSERT_NEAR(accel.ax_g, -1.0f, 0.01f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_who_am_i_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, MPU6050_REG_WHO_AM_I, 0x00);
    ASSERT_EQ(model.hw.regs[MPU6050_REG_WHO_AM_I], 0x68);
}

TEST(test_data_registers_readonly)
{
    setup();
    mpu6050_model_set_accel(&model, 0, 0, 16384);
    uint8_t original = model.hw.regs[MPU6050_REG_ACCEL_XOUT_H];
    hw_model__write_reg(&model.hw, MPU6050_REG_ACCEL_XOUT_H, 0xFF);
    ASSERT_EQ(model.hw.regs[MPU6050_REG_ACCEL_XOUT_H], original);
}

TEST(test_reset_via_driver)
{
    setup();
    mpu6050_t dev;
    mpu6050_init(&dev, &model_i2c, &default_cfg);

    /* Write something to a config register */
    hw_model__write_reg(&model.hw, MPU6050_REG_SMPLRT_DIV, 0x07);
    ASSERT_EQ(model.hw.regs[MPU6050_REG_SMPLRT_DIV], 0x07);

    int before = model.reset_count;
    mpu6050_reset(&dev);
    ASSERT_TRUE(model.reset_count > before);
    /* After reset, SMPLRT_DIV back to default 0 */
    ASSERT_EQ(model.hw.regs[MPU6050_REG_SMPLRT_DIV], 0x00);
}

TEST(test_different_accel_values)
{
    setup();
    mpu6050_t dev;
    mpu6050_init(&dev, &model_i2c, &default_cfg);

    mpu6050_model_set_accel(&model, 4000, 0, 16384);
    mpu6050_accel_t a1;
    mpu6050_read_accel(&dev, &a1);

    mpu6050_model_set_accel(&model, 8000, 0, 16384);
    mpu6050_accel_t a2;
    mpu6050_read_accel(&dev, &a2);

    ASSERT_TRUE(a2.ax_g > a1.ax_g);
}

TEST_MAIN("mpu6050 hardware model")
    RUN(test_init_reads_who_am_i);
    RUN(test_init_wrong_who_am_i);
    RUN(test_init_writes_accel_config);
    RUN(test_init_writes_gyro_config);
    RUN(test_init_resets_device);
    RUN(test_read_accel_1g_z);
    RUN(test_read_gyro_zero);
    RUN(test_read_accel_negative);
    RUN(test_who_am_i_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_reset_via_driver);
    RUN(test_different_accel_values);
TEST_MAIN_END()
