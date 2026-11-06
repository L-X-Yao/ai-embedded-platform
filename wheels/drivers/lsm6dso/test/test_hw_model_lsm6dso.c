// SPDX-License-Identifier: MIT
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

/* STATUS_REG, DS12140 Table 78 p64 */
#define XLDA 0x01u
#define GDA  0x02u
#define TDA  0x04u

static uint8_t read_status(void)
{
    uint8_t status = 0xFFu;
    hw_model_read_regs(&model, 0, 0x1E, &status, 1);
    return status;
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
    /* CTRL2_G: ODR=4(104Hz), FS_G[3:0]=8(1000DPS) -> (4<<4)|8 = 0x48 */
    ASSERT_EQ(model.regs[0x11], (uint8_t)((4 << 4) | 8));
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

TEST(test_init_keeps_the_address_increment)
{
    setup();
    lsm6dso_t dev;
    ASSERT_EQ(lsm6dso_init(&dev, &model_i2c, &default_cfg), LSM6DSO_OK);
    ASSERT_EQ(model.regs[0x12] & 0x44, 0x44);

    /* Each axis has to come from its own register pair */
    lsm6dso_model_set_accel(&model, 1000, 2000, 3000);
    lsm6dso_accel_data_t accel;
    ASSERT_EQ(lsm6dso_read_accel(&dev, &accel), LSM6DSO_OK);
    ASSERT_NEAR(accel.x_mg, 61.0f, 0.1f);
    ASSERT_NEAR(accel.y_mg, 122.0f, 0.1f);
    ASSERT_NEAR(accel.z_mg, 183.0f, 0.1f);
}

/* --- what the model itself must do (DS12140) ----------------------------- */

/* p40 Table 20: "CTRL3_C RW 12 00010010 00000100", "CTRL9_XL RW 18 00011000
 * 11100000" */
TEST(test_table_20_defaults)
{
    setup();
    ASSERT_EQ_HEX(model.regs[0x0F], 0x6Cu);
    ASSERT_EQ_HEX(model.regs[0x12], 0x04u);
    ASSERT_EQ_HEX(model.regs[0x18], 0xE0u);
}

/* p52: SW_RESET "This bit is automatically cleared." */
TEST(test_sw_reset_clears_itself)
{
    setup();
    hw_model_write_reg(&model, 0, 0x12, 0x45u);
    ASSERT_EQ_HEX(model.regs[0x12], 0x04u);
    ASSERT_EQ(model.reset_count, 1);
}

/* p52: BOOT "This bit is automatically cleared." */
TEST(test_boot_clears_itself)
{
    setup();
    hw_model_write_reg(&model, 0, 0x12, 0x84u);
    ASSERT_EQ_HEX(model.regs[0x12], 0x04u);
}

/* p64: every flag has "Default value: 0" and the register is read-only */
TEST(test_status_is_clear_and_read_only)
{
    setup();
    ASSERT_EQ_HEX(read_status(), 0u);
    hw_model_write_reg(&model, 0, 0x1E, 0xFFu);
    ASSERT_EQ_HEX(read_status(), 0u);
}

/* p64: XLDA "1: a new set of data is available at accelerometer output" */
TEST(test_accel_sample_sets_xlda_only)
{
    setup();
    lsm6dso_t dev;
    ASSERT_EQ(lsm6dso_init(&dev, &model_i2c, &default_cfg), LSM6DSO_OK);
    lsm6dso_model_set_accel(&model, 1, 2, 3);
    ASSERT_EQ_HEX(read_status(), XLDA);
}

/* p64: GDA "1: a new set of data is available at gyroscope output" */
TEST(test_gyro_sample_sets_gda_only)
{
    setup();
    lsm6dso_t dev;
    ASSERT_EQ(lsm6dso_init(&dev, &model_i2c, &default_cfg), LSM6DSO_OK);
    lsm6dso_model_set_gyro(&model, 1, 2, 3);
    ASSERT_EQ_HEX(read_status(), GDA);
}

/* p64: TDA "1: a new set of data is available at temperature sensor output" */
TEST(test_temp_sample_sets_tda_only)
{
    setup();
    lsm6dso_t dev;
    ASSERT_EQ(lsm6dso_init(&dev, &model_i2c, &default_cfg), LSM6DSO_OK);
    lsm6dso_model_set_temp(&model, 256);
    ASSERT_EQ_HEX(read_status(), TDA);
}

/* p64 Table 81: "reset when one of the high parts of the output data is read" */
TEST(test_read_clears_only_that_flag)
{
    setup();
    lsm6dso_t dev;
    ASSERT_EQ(lsm6dso_init(&dev, &model_i2c, &default_cfg), LSM6DSO_OK);
    lsm6dso_model_set_accel(&model, 1, 2, 3);
    lsm6dso_model_set_gyro(&model, 1, 2, 3);
    lsm6dso_model_set_temp(&model, 256);
    ASSERT_EQ_HEX(read_status(), XLDA | GDA | TDA);

    lsm6dso_accel_data_t accel;
    ASSERT_EQ(lsm6dso_read_accel(&dev, &accel), LSM6DSO_OK);
    ASSERT_EQ_HEX(read_status(), GDA | TDA);

    lsm6dso_gyro_data_t gyro;
    ASSERT_EQ(lsm6dso_read_gyro(&dev, &gyro), LSM6DSO_OK);
    ASSERT_EQ_HEX(read_status(), TDA);

    float temp;
    ASSERT_EQ(lsm6dso_read_temp(&dev, &temp), LSM6DSO_OK);
    ASSERT_EQ_HEX(read_status(), 0u);
}

/* The low byte alone is not one of the "high parts" */
TEST(test_low_byte_keeps_the_flag)
{
    setup();
    lsm6dso_t dev;
    ASSERT_EQ(lsm6dso_init(&dev, &model_i2c, &default_cfg), LSM6DSO_OK);
    lsm6dso_model_set_accel(&model, 1, 2, 3);
    uint8_t low = 0;
    hw_model_read_regs(&model, 0, 0x28, &low, 1);
    ASSERT_EQ_HEX(read_status(), XLDA);
    uint8_t high = 0;
    hw_model_read_regs(&model, 0, 0x29, &high, 1);
    ASSERT_EQ_HEX(read_status(), 0u);
}

/* p27: each sensor "is activated from power-down by writing" its ODR field, so
 * with both at 0 no sample reaches the output registers */
TEST(test_power_down_makes_no_sample)
{
    setup();
    lsm6dso_model_set_accel(&model, 0x1234, 0, 0);
    lsm6dso_model_set_gyro(&model, 0x1234, 0, 0);
    lsm6dso_model_set_temp(&model, 0x1234);
    ASSERT_EQ_HEX(read_status(), 0u);
    ASSERT_EQ_HEX(model.regs[0x28], 0u);
    ASSERT_EQ_HEX(model.regs[0x22], 0u);
    ASSERT_EQ_HEX(model.regs[0x20], 0u);
}

/* The two ODR fields are separate: p27 "the ODRs are totally independent" */
TEST(test_each_odr_gates_its_own_sensor)
{
    setup();
    hw_model_write_reg(&model, 0, 0x10, 0x40u);  /* accel on, gyro off */
    lsm6dso_model_set_gyro(&model, 0x1234, 0, 0);
    ASSERT_EQ_HEX(read_status(), 0u);
    ASSERT_EQ_HEX(model.regs[0x22], 0u);
    lsm6dso_model_set_accel(&model, 0x1234, 0, 0);
    ASSERT_EQ_HEX(read_status(), XLDA);

    setup();
    hw_model_write_reg(&model, 0, 0x11, 0x40u);  /* gyro on, accel off */
    lsm6dso_model_set_accel(&model, 0x1234, 0, 0);
    ASSERT_EQ_HEX(read_status(), 0u);
    ASSERT_EQ_HEX(model.regs[0x28], 0u);
    lsm6dso_model_set_gyro(&model, 0x1234, 0, 0);
    ASSERT_EQ_HEX(read_status(), GDA);
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
    RUN(test_init_keeps_the_address_increment);
    RUN(test_table_20_defaults);
    RUN(test_sw_reset_clears_itself);
    RUN(test_boot_clears_itself);
    RUN(test_status_is_clear_and_read_only);
    RUN(test_accel_sample_sets_xlda_only);
    RUN(test_gyro_sample_sets_gda_only);
    RUN(test_temp_sample_sets_tda_only);
    RUN(test_read_clears_only_that_flag);
    RUN(test_low_byte_keeps_the_flag);
    RUN(test_power_down_makes_no_sample);
    RUN(test_each_odr_gates_its_own_sensor);
TEST_MAIN_END()
