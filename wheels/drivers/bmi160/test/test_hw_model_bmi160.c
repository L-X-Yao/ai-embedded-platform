/*
 * test_hw_model_bmi160.c — tests using hardware register model
 *
 * Validates BMI160 driver against a datasheet-accurate register model.
 * The model enforces chip ID read-only, data registers read-only,
 * CMD-driven power mode, and soft-reset behavior.
 */

#include "test_harness.h"
#include "hw_model_bmi160.h"
#include "../bmi160.h"
#include <math.h>

static bmi160_model_t model;

static const bmi160_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const bmi160_cfg_t default_cfg = {
    .addr        = BMI160_ADDR_LOW,
    .accel_range = BMI160_ACCEL_2G,
    .gyro_range  = BMI160_GYRO_250DPS,
};

static void setup(void)
{
    bmi160_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_reads_chip_id)
{
    setup();
    bmi160_t dev;
    bmi160_err_t err = bmi160_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BMI160_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[BMI160_REG_CHIP_ID] = 0xFF;
    hw_model_reset(&model.hw);
    bmi160_model_set_data_regs(&model);

    bmi160_t dev;
    bmi160_err_t err = bmi160_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BMI160_ERR_ID);
}

TEST(test_init_writes_acc_range)
{
    setup();
    bmi160_t dev;
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_8G,
                         .gyro_range = BMI160_GYRO_250DPS };
    bmi160_init(&dev, &model_i2c, &cfg);
    ASSERT_EQ(model.hw.regs[BMI160_REG_ACC_RANGE], (int)BMI160_ACCEL_8G);
}

TEST(test_init_writes_gyr_range)
{
    setup();
    bmi160_t dev;
    bmi160_cfg_t cfg = { .addr = BMI160_ADDR_LOW,
                         .accel_range = BMI160_ACCEL_2G,
                         .gyro_range = BMI160_GYRO_1000DPS };
    bmi160_init(&dev, &model_i2c, &cfg);
    ASSERT_EQ(model.hw.regs[BMI160_REG_GYR_RANGE], (int)BMI160_GYRO_1000DPS);
}

TEST(test_init_sets_normal_mode)
{
    setup();
    bmi160_t dev;
    bmi160_init(&dev, &model_i2c, &default_cfg);
    /* After init, both accel and gyro should be in normal mode */
    uint8_t pmu = model.hw.regs[BMI160_REG_PMU_STATUS];
    /* acc_pmu[5:4] = 01 */
    ASSERT_EQ((pmu >> 4) & 0x03, 0x01);
    /* gyr_pmu[3:2] = 01 */
    ASSERT_EQ((pmu >> 2) & 0x03, 0x01);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_accel_1g_z)
{
    setup();
    bmi160_t dev;
    bmi160_init(&dev, &model_i2c, &default_cfg);
    /* At 2g range, 0.061 mg/LSB: +1g = 1000 mg / 0.061 = 16393 raw */
    bmi160_model_set_accel(&model, 0, 0, 16393);

    bmi160_data_t data;
    bmi160_err_t err = bmi160_read(&dev, &data);
    ASSERT_EQ(err, BMI160_OK);
    ASSERT_NEAR(data.accel_mg[0], 0.0f, 1.0f);
    ASSERT_NEAR(data.accel_mg[1], 0.0f, 1.0f);
    ASSERT_NEAR(data.accel_mg[2], 1000.0f, 1.0f);
}

TEST(test_read_gyro_zero)
{
    setup();
    bmi160_t dev;
    bmi160_init(&dev, &model_i2c, &default_cfg);
    bmi160_model_set_gyro(&model, 0, 0, 0);

    bmi160_data_t data;
    bmi160_err_t err = bmi160_read(&dev, &data);
    ASSERT_EQ(err, BMI160_OK);
    ASSERT_NEAR(data.gyro_dps[0], 0.0f, 0.01f);
    ASSERT_NEAR(data.gyro_dps[1], 0.0f, 0.01f);
    ASSERT_NEAR(data.gyro_dps[2], 0.0f, 0.01f);
}

TEST(test_read_accel_negative)
{
    setup();
    bmi160_t dev;
    bmi160_init(&dev, &model_i2c, &default_cfg);
    /* -1g on X */
    bmi160_model_set_accel(&model, -16393, 0, 0);

    bmi160_data_t data;
    bmi160_read(&dev, &data);
    ASSERT_NEAR(data.accel_mg[0], -1000.0f, 1.0f);
}

/* --- roundtrip: inject raw, read through driver, verify conversion --- */

TEST(test_roundtrip_accel_all_axes)
{
    setup();
    bmi160_t dev;
    bmi160_init(&dev, &model_i2c, &default_cfg);

    /* Known raw values at 2g range (0.061 mg/LSB) */
    int16_t raw_x = 1000;   /* expect ~61 mg */
    int16_t raw_y = -2000;  /* expect ~-122 mg */
    int16_t raw_z = 16393;  /* expect ~1000 mg */
    bmi160_model_set_accel(&model, raw_x, raw_y, raw_z);

    bmi160_data_t data;
    bmi160_read(&dev, &data);
    ASSERT_NEAR(data.accel_mg[0], (float)raw_x * 0.061f, 0.1f);
    ASSERT_NEAR(data.accel_mg[1], (float)raw_y * 0.061f, 0.1f);
    ASSERT_NEAR(data.accel_mg[2], (float)raw_z * 0.061f, 0.1f);
}

TEST(test_roundtrip_gyro_all_axes)
{
    setup();
    bmi160_t dev;
    bmi160_init(&dev, &model_i2c, &default_cfg);

    /* Known raw values at 250 dps range (0.00763 dps/LSB) */
    int16_t raw_x = 1000;   /* expect ~7.63 dps */
    int16_t raw_y = -500;   /* expect ~-3.815 dps */
    int16_t raw_z = 131;    /* expect ~1.0 dps */
    bmi160_model_set_gyro(&model, raw_x, raw_y, raw_z);

    bmi160_data_t data;
    bmi160_read(&dev, &data);
    ASSERT_NEAR(data.gyro_dps[0], (float)raw_x * 0.00763f, 0.01f);
    ASSERT_NEAR(data.gyro_dps[1], (float)raw_y * 0.00763f, 0.01f);
    ASSERT_NEAR(data.gyro_dps[2], (float)raw_z * 0.00763f, 0.01f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, BMI160_REG_CHIP_ID, 0x00);
    ASSERT_EQ(model.hw.regs[BMI160_REG_CHIP_ID], 0xD1);
}

TEST(test_data_registers_readonly)
{
    setup();
    bmi160_model_set_accel(&model, 0, 0, 16384);
    uint8_t original = model.hw.regs[BMI160_REG_ACC_DATA_XL];
    hw_model__write_reg(&model.hw, BMI160_REG_ACC_DATA_XL, 0xFF);
    ASSERT_EQ(model.hw.regs[BMI160_REG_ACC_DATA_XL], original);
}

TEST(test_reset_via_driver)
{
    setup();
    bmi160_t dev;
    bmi160_init(&dev, &model_i2c, &default_cfg);

    /* Write something to a config register */
    hw_model__write_reg(&model.hw, BMI160_REG_ACC_RANGE, 0x08);
    ASSERT_EQ(model.hw.regs[BMI160_REG_ACC_RANGE], 0x08);

    int before = model.reset_count;
    bmi160_reset(&dev);
    ASSERT_TRUE(model.reset_count > before);
    /* After reset, ACC_RANGE back to POR default 0x03 */
    ASSERT_EQ(model.hw.regs[BMI160_REG_ACC_RANGE], BMI160_POR_ACC_RANGE);
}

TEST(test_different_accel_values)
{
    setup();
    bmi160_t dev;
    bmi160_init(&dev, &model_i2c, &default_cfg);

    bmi160_model_set_accel(&model, 4000, 0, 16384);
    bmi160_data_t d1;
    bmi160_read(&dev, &d1);

    bmi160_model_set_accel(&model, 8000, 0, 16384);
    bmi160_data_t d2;
    bmi160_read(&dev, &d2);

    ASSERT_TRUE(d2.accel_mg[0] > d1.accel_mg[0]);
}

TEST_MAIN("bmi160 hardware model")
    RUN(test_init_reads_chip_id);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_writes_acc_range);
    RUN(test_init_writes_gyr_range);
    RUN(test_init_sets_normal_mode);
    RUN(test_read_accel_1g_z);
    RUN(test_read_gyro_zero);
    RUN(test_read_accel_negative);
    RUN(test_roundtrip_accel_all_axes);
    RUN(test_roundtrip_gyro_all_axes);
    RUN(test_chip_id_readonly);
    RUN(test_data_registers_readonly);
    RUN(test_reset_via_driver);
    RUN(test_different_accel_values);
TEST_MAIN_END()
