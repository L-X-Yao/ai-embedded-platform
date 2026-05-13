/*
 * test_hw_model_lsm6dso.c — tests using hardware register model
 *
 * Validates LSM6DSO driver against a datasheet-accurate register model.
 * The model enforces WHO_AM_I read-only, SW_RESET via CTRL3_C,
 * data register read-only, and accel/gyro/temp scaling.
 */

#include "test_harness.h"
#include "hw_model_lsm6dso.h"
#include "../lsm6dso.h"

static lsm6dso_model_t model;

static const lsm6dso_i2c_t model_i2c = {
    .write_reg = hw_model_write_reg,
    .read_regs = hw_model_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model,
};

static const lsm6dso_cfg_t default_cfg = {
    .addr        = LSM6DSO_ADDR_LOW,
    .accel_range = LSM6DSO_ACCEL_2G,
    .gyro_range  = LSM6DSO_GYRO_250DPS,
    .odr         = LSM6DSO_ODR_104HZ,
};

static void setup(void)
{
    lsm6dso_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_reads_who_am_i)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_err_t err = lsm6dso_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, LSM6DSO_OK);
}

TEST(test_init_wrong_who_am_i)
{
    setup();
    model.reset_values[0x0F] = 0xFF;
    lsm6dso_model_reset_regs(&model);

    lsm6dso_t dev;
    lsm6dso_err_t err = lsm6dso_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, LSM6DSO_ERR_ID);
}

TEST(test_init_writes_ctrl1_xl)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_cfg_t cfg = { .addr = LSM6DSO_ADDR_LOW,
                           .accel_range = LSM6DSO_ACCEL_8G,
                           .gyro_range = LSM6DSO_GYRO_250DPS,
                           .odr = LSM6DSO_ODR_104HZ };
    lsm6dso_init(&dev, &model_i2c, &cfg);
    /* CTRL1_XL: ODR=4(104Hz), FS_XL=3(8G) -> (4<<4)|(3<<2) = 0x4C */
    ASSERT_EQ(model.regs[0x10], (uint8_t)((4 << 4) | (3 << 2)));
}

TEST(test_init_writes_ctrl2_g)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_cfg_t cfg = { .addr = LSM6DSO_ADDR_LOW,
                           .accel_range = LSM6DSO_ACCEL_2G,
                           .gyro_range = LSM6DSO_GYRO_1000DPS,
                           .odr = LSM6DSO_ODR_104HZ };
    lsm6dso_init(&dev, &model_i2c, &cfg);
    /* CTRL2_G: ODR=4(104Hz), FS_G=8(1000DPS) -> (4<<4)|(8<<1) = 0x50 */
    ASSERT_EQ(model.regs[0x11], (uint8_t)((4 << 4) | (8 << 1)));
}

TEST(test_init_triggers_sw_reset)
{
    setup();
    lsm6dso_t dev;
    int before = model.reset_count;
    lsm6dso_init(&dev, &model_i2c, &default_cfg);
    ASSERT_TRUE(model.reset_count > before);
}

TEST(test_init_sets_bdu)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_init(&dev, &model_i2c, &default_cfg);
    /* CTRL3_C should have BDU bit (0x40) set */
    ASSERT_EQ(model.regs[0x12] & 0x40, 0x40);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_accel_1g_z)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_init(&dev, &model_i2c, &default_cfg);
    /* Model default: az = 16393 raw, at 2G range (0.061 mg/LSB) -> ~1000 mg */
    lsm6dso_model_set_accel(&model, 0, 0, 16393);

    lsm6dso_accel_data_t accel;
    lsm6dso_err_t err = lsm6dso_read_accel(&dev, &accel);
    ASSERT_EQ(err, LSM6DSO_OK);
    ASSERT_NEAR(accel.x_mg, 0.0f, 1.0f);
    ASSERT_NEAR(accel.y_mg, 0.0f, 1.0f);
    ASSERT_NEAR(accel.z_mg, 1000.0f, 2.0f);
}

TEST(test_read_accel_negative)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_init(&dev, &model_i2c, &default_cfg);
    lsm6dso_model_set_accel(&model, -16393, 0, 0);

    lsm6dso_accel_data_t accel;
    lsm6dso_read_accel(&dev, &accel);
    ASSERT_NEAR(accel.x_mg, -1000.0f, 2.0f);
}

TEST(test_read_gyro_zero)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_init(&dev, &model_i2c, &default_cfg);
    lsm6dso_model_set_gyro(&model, 0, 0, 0);

    lsm6dso_gyro_data_t gyro;
    lsm6dso_err_t err = lsm6dso_read_gyro(&dev, &gyro);
    ASSERT_EQ(err, LSM6DSO_OK);
    ASSERT_NEAR(gyro.x_dps, 0.0f, 0.01f);
    ASSERT_NEAR(gyro.y_dps, 0.0f, 0.01f);
    ASSERT_NEAR(gyro.z_dps, 0.0f, 0.01f);
}

TEST(test_read_temp_25c)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_init(&dev, &model_i2c, &default_cfg);
    lsm6dso_model_set_temp(&model, 0);  /* raw=0 -> 25C */

    float temp;
    lsm6dso_err_t err = lsm6dso_read_temp(&dev, &temp);
    ASSERT_EQ(err, LSM6DSO_OK);
    ASSERT_NEAR(temp, 25.0f, 0.01f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_who_am_i_readonly)
{
    setup();
    hw_model_write_reg(&model, 0, 0x0F, 0x00);
    ASSERT_EQ(model.regs[0x0F], 0x6C);
}

TEST(test_data_registers_readonly)
{
    setup();
    lsm6dso_model_set_accel(&model, 0, 0, 16393);
    uint8_t original = model.regs[0x28];
    hw_model_write_reg(&model, 0, 0x28, 0xFF);
    ASSERT_EQ(model.regs[0x28], original);
}

TEST(test_reset_via_driver)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_init(&dev, &model_i2c, &default_cfg);

    /* Write something to a config register */
    hw_model_write_reg(&model, 0, 0x10, 0x77);
    ASSERT_EQ(model.regs[0x10], 0x77);

    int before = model.reset_count;
    lsm6dso_reset(&dev);
    ASSERT_TRUE(model.reset_count > before);
    /* After reset, CTRL1_XL back to default 0 */
    ASSERT_EQ(model.regs[0x10], 0x00);
}

TEST(test_different_accel_values)
{
    setup();
    lsm6dso_t dev;
    lsm6dso_init(&dev, &model_i2c, &default_cfg);

    lsm6dso_model_set_accel(&model, 4000, 0, 16393);
    lsm6dso_accel_data_t a1;
    lsm6dso_read_accel(&dev, &a1);

    lsm6dso_model_set_accel(&model, 8000, 0, 16393);
    lsm6dso_accel_data_t a2;
    lsm6dso_read_accel(&dev, &a2);

    ASSERT_TRUE(a2.x_mg > a1.x_mg);
}

TEST_MAIN("lsm6dso hardware model")
    RUN(test_init_reads_who_am_i);
    RUN(test_init_wrong_who_am_i);
    RUN(test_init_writes_ctrl1_xl);
    RUN(test_init_writes_ctrl2_g);
    RUN(test_init_triggers_sw_reset);
    RUN(test_init_sets_bdu);
    RUN(test_read_accel_1g_z);
    RUN(test_read_accel_negative);
    RUN(test_read_gyro_zero);
    RUN(test_read_temp_25c);
    RUN(test_who_am_i_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_reset_via_driver);
    RUN(test_different_accel_values);
TEST_MAIN_END()
