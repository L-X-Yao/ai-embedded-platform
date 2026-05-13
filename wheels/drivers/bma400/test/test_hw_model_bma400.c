/*
 * test_hw_model_bma400.c — tests using hardware register model
 *
 * Validates BMA400 driver against a datasheet-accurate register model.
 * The model enforces chip ID read-only, data registers read-only,
 * writable masks on config registers, and acceleration scaling.
 */

#include "test_harness.h"
#include "hw_model_bma400.h"
#include "../bma400.h"
#include <math.h>

static bma400_model_t model;

static const bma400_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const bma400_cfg_t default_cfg = {
    .addr  = BMA400_ADDR_LOW,
    .range = BMA400_RANGE_2G,
    .power = BMA400_POWER_NORMAL,
};

static void setup(void)
{
    bma400_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_hw_init_por_defaults)
{
    setup();
    bma400_t dev;
    bma400_err_t err = bma400_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BMA400_OK);
}

TEST(test_hw_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[BMA400_REG_CHIP_ID] = 0xFF;
    hw_model_reset(&model.hw);
    bma400_model_set_data_regs(&model);

    bma400_t dev;
    bma400_err_t err = bma400_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BMA400_ERR_ID);
}

TEST(test_hw_init_writes_power_mode)
{
    setup();
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &default_cfg);
    /* ACC_CONFIG0 bits [1:0] = 2 (normal mode) */
    ASSERT_EQ(model.hw.regs[BMA400_REG_ACC_CONFIG0] & 0x03, 0x02);
}

TEST(test_hw_init_writes_range)
{
    setup();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_8G,
                         .power = BMA400_POWER_NORMAL };
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &cfg);
    /* ACC_CONFIG1 bits [7:6] = 2 (8g) -> 0x80 */
    ASSERT_EQ((model.hw.regs[BMA400_REG_ACC_CONFIG1] >> 6) & 0x03, 2);
}

/* --- chip ID read-only ------------------------------------------------- */

TEST(test_hw_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, BMA400_REG_CHIP_ID, 0x00);
    ASSERT_EQ(model.hw.regs[BMA400_REG_CHIP_ID], 0x90);
}

/* --- data registers read-only ------------------------------------------ */

TEST(test_hw_data_registers_readonly)
{
    setup();
    bma400_model_set_accel_raw(&model, 100, 200, 1024);
    uint8_t original = model.hw.regs[BMA400_REG_ACC_Z_LSB];
    hw_model__write_reg(&model.hw, BMA400_REG_ACC_Z_LSB, 0xFF);
    ASSERT_EQ(model.hw.regs[BMA400_REG_ACC_Z_LSB], original);
}

/* --- read round-trip tests --------------------------------------------- */

TEST(test_hw_read_1g_z)
{
    setup();
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &default_cfg);
    /* Model default: z=1024 at 2g -> 1000 mg */
    bma400_data_t data;
    bma400_err_t err = bma400_read(&dev, &data);
    ASSERT_EQ(err, BMA400_OK);
    ASSERT_NEAR(data.x_mg, 0.0f, 1.0f);
    ASSERT_NEAR(data.y_mg, 0.0f, 1.0f);
    ASSERT_NEAR(data.z_mg, 1000.0f, 1.0f);
}

TEST(test_hw_read_negative_accel)
{
    setup();
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &default_cfg);
    bma400_model_set_accel_raw(&model, -1024, 0, 0);
    bma400_data_t data;
    bma400_read(&dev, &data);
    ASSERT_NEAR(data.x_mg, -1000.0f, 1.0f);
}

TEST(test_hw_inject_accel_mg)
{
    setup();
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &default_cfg);
    bma400_model_inject_accel_mg(&model, 500.0f, -250.0f, 981.0f);
    bma400_data_t data;
    bma400_read(&dev, &data);
    ASSERT_NEAR(data.x_mg, 500.0f, 5.0f);
    ASSERT_NEAR(data.y_mg, -250.0f, 5.0f);
    ASSERT_NEAR(data.z_mg, 981.0f, 5.0f);
}

/* --- range change with hw model ---------------------------------------- */

TEST(test_hw_set_range_updates_config1)
{
    setup();
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &default_cfg);

    bma400_set_range(&dev, BMA400_RANGE_16G);
    ASSERT_EQ((model.hw.regs[BMA400_REG_ACC_CONFIG1] >> 6) & 0x03, 3);
}

TEST(test_hw_config1_mask)
{
    setup();
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &default_cfg);

    /* Writing to ACC_CONFIG1 should only affect bits [7:6] */
    /* The driver writes (range << 6), so lower bits stay 0 from init.
     * Verify mask enforcement: attempt to write full 0xFF via model */
    hw_model__write_reg(&model.hw, BMA400_REG_ACC_CONFIG1, 0xFF);
    /* Only bits [7:6] should be writable */
    ASSERT_EQ(model.hw.regs[BMA400_REG_ACC_CONFIG1] & 0x3F, 0x00);
}

/* --- power mode with hw model ------------------------------------------ */

TEST(test_hw_set_power_updates_config0)
{
    setup();
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &default_cfg);

    bma400_set_power(&dev, BMA400_POWER_SLEEP);
    ASSERT_EQ(model.hw.regs[BMA400_REG_ACC_CONFIG0] & 0x03, 0x00);

    bma400_set_power(&dev, BMA400_POWER_LOW_POWER);
    ASSERT_EQ(model.hw.regs[BMA400_REG_ACC_CONFIG0] & 0x03, 0x01);
}

TEST(test_hw_config0_mask)
{
    setup();
    /* Writing to ACC_CONFIG0 should only affect bits [1:0] */
    hw_model__write_reg(&model.hw, BMA400_REG_ACC_CONFIG0, 0xFF);
    ASSERT_EQ(model.hw.regs[BMA400_REG_ACC_CONFIG0] & 0xFC, 0x00);
}

/* --- different range scaling ------------------------------------------- */

TEST(test_hw_read_4g_range)
{
    setup();
    bma400_cfg_t cfg = { .addr = BMA400_ADDR_LOW,
                         .range = BMA400_RANGE_4G,
                         .power = BMA400_POWER_NORMAL };
    bma400_t dev;
    bma400_init(&dev, &model_i2c, &cfg);
    /* Inject 1g at 4g range: raw=512 */
    bma400_model_set_accel_raw(&model, 512, 0, 0);
    bma400_data_t data;
    bma400_read(&dev, &data);
    ASSERT_NEAR(data.x_mg, 1000.0f, 2.0f);
}

TEST_MAIN("bma400_hw_model")
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_init_wrong_chip_id);
    RUN(test_hw_init_writes_power_mode);
    RUN(test_hw_init_writes_range);
    RUN(test_hw_chip_id_readonly);
    RUN(test_hw_data_registers_readonly);
    RUN(test_hw_read_1g_z);
    RUN(test_hw_read_negative_accel);
    RUN(test_hw_inject_accel_mg);
    RUN(test_hw_set_range_updates_config1);
    RUN(test_hw_config1_mask);
    RUN(test_hw_set_power_updates_config0);
    RUN(test_hw_config0_mask);
    RUN(test_hw_read_4g_range);
TEST_MAIN_END()
