// SPDX-License-Identifier: MIT
/*
 * test_hw_model_vl53l1x.c — tests using hardware register model
 *
 * Validates VL53L1X driver against a datasheet-accurate register model.
 * Focus on register-level behavior: POR defaults, read-only enforcement,
 * measurement engine side effects, and distance inject round-trips.
 */

#include "test_harness.h"
#include "hw_model_vl53l1x.h"
#include "vl53l1x.h"

static vl53l1x_model_t model;

static const vl53l1x_i2c_t model_i2c = {
    .write_reg = vl53l1x_model_write_reg,
    .read_regs = vl53l1x_model_read_regs,
    .delay_ms  = vl53l1x_model_delay_ms,
    .ctx       = &model,
};

static const vl53l1x_cfg_t default_cfg = {
    .addr = VL53L1X_ADDR_DEFAULT,
};

static vl53l1x_t dev;

static void setup(void)
{
    vl53l1x_model_init(&model);
}

/* --- POR default tests ------------------------------------------------ */

TEST(test_hw_por_model_id)
{
    setup();
    ASSERT_EQ(model.regs[VL1X_REG_MODEL_ID], 0xEA);
}

TEST(test_hw_por_module_type)
{
    setup();
    ASSERT_EQ(model.regs[VL1X_REG_MODULE_TYPE], 0xCC);
}

TEST(test_hw_init_por_defaults)
{
    setup();
    vl53l1x_err_t err = vl53l1x_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, VL53L1X_OK);
}

/* --- round-trip: inject distance, read back via driver --- */

TEST(test_hw_inject_distance_250)
{
    setup();
    vl53l1x_init(&dev, &model_i2c, &default_cfg);
    vl53l1x_model_inject_distance(&model, 250);

    uint16_t dist = 0;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 250);
}

TEST(test_hw_inject_distance_0)
{
    setup();
    vl53l1x_init(&dev, &model_i2c, &default_cfg);
    vl53l1x_model_inject_distance(&model, 0);

    uint16_t dist = 9999;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 0);
}

TEST(test_hw_inject_distance_4000)
{
    setup();
    vl53l1x_init(&dev, &model_i2c, &default_cfg);
    vl53l1x_model_inject_distance(&model, 4000);

    uint16_t dist = 0;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 4000);
}

TEST(test_hw_inject_distance_max_u16)
{
    setup();
    vl53l1x_init(&dev, &model_i2c, &default_cfg);
    vl53l1x_model_inject_distance(&model, 0xFFFF);

    uint16_t dist = 0;
    vl53l1x_err_t err = vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(err, VL53L1X_OK);
    ASSERT_EQ(dist, 0xFFFF);
}

/* --- measurement engine side effect --- */

TEST(test_hw_start_ranging_triggers_engine)
{
    setup();
    vl53l1x_init(&dev, &model_i2c, &default_cfg);
    model.true_range_mm = 1234;

    int before = model.measure_count;
    vl53l1x_start_ranging(&dev);
    ASSERT_EQ(model.measure_count, before + 1);

    /* Engine should have populated result registers */
    uint16_t dist = 0;
    vl53l1x_read_distance(&dev, &dist);
    ASSERT_EQ(dist, 1234);
}

TEST(test_hw_stop_ranging_clears_start)
{
    setup();
    vl53l1x_init(&dev, &model_i2c, &default_cfg);
    vl53l1x_start_ranging(&dev);
    ASSERT_EQ(model.regs[VL1X_REG_SYSTEM_START], 0x40);

    vl53l1x_stop_ranging(&dev);
    ASSERT_EQ(model.regs[VL1X_REG_SYSTEM_START], 0x00);
}

/* --- readonly enforcement --- */

TEST(test_hw_model_id_immutable)
{
    setup();
    /* Direct write to model ID must not change it */
    vl53l1x_model_write_reg(&model, 0x29, VL1X_REG_MODEL_ID, 0x00);
    ASSERT_EQ(model.regs[VL1X_REG_MODEL_ID], 0xEA);
}

TEST(test_hw_module_type_immutable)
{
    setup();
    vl53l1x_model_write_reg(&model, 0x29, VL1X_REG_MODULE_TYPE, 0x00);
    ASSERT_EQ(model.regs[VL1X_REG_MODULE_TYPE], 0xCC);
}

/* --- reset restores POR --- */

TEST(test_hw_reset_restores_defaults)
{
    setup();
    vl53l1x_init(&dev, &model_i2c, &default_cfg);
    vl53l1x_start_ranging(&dev);

    /* Reset should restore POR values */
    vl53l1x_model_reset(&model);
    ASSERT_EQ(model.regs[VL1X_REG_MODEL_ID], 0xEA);
    ASSERT_EQ(model.regs[VL1X_REG_MODULE_TYPE], 0xCC);
    ASSERT_EQ(model.regs[VL1X_REG_SYSTEM_START], 0x00);
    ASSERT_EQ(model.measure_count, 0);
}

TEST_MAIN("vl53l1x_hw_model")
    RUN(test_hw_por_model_id);
    RUN(test_hw_por_module_type);
    RUN(test_hw_init_por_defaults);
    RUN(test_hw_inject_distance_250);
    RUN(test_hw_inject_distance_0);
    RUN(test_hw_inject_distance_4000);
    RUN(test_hw_inject_distance_max_u16);
    RUN(test_hw_start_ranging_triggers_engine);
    RUN(test_hw_stop_ranging_clears_start);
    RUN(test_hw_model_id_immutable);
    RUN(test_hw_module_type_immutable);
    RUN(test_hw_reset_restores_defaults);
TEST_MAIN_END()
