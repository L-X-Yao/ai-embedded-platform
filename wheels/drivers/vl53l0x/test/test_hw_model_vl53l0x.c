/*
 * test_hw_model_vl53l0x.c — tests using hardware register model
 *
 * Validates VL53L0X driver against a datasheet-accurate register model.
 * The model enforces read-only model ID (0xEE), range data registers,
 * single-shot measurement trigger, and timing budget configuration.
 */

#include "test_harness.h"
#include "hw_model_vl53l0x.h"
#include "../vl53l0x.h"
#include <math.h>

static vl53l0x_model_t model;

static const vl53l0x_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const vl53l0x_cfg_t default_cfg = {
    .addr = VL53L0X_ADDR_DEFAULT,
};

static void setup(void)
{
    vl53l0x_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_err_t err = vl53l0x_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, VL53L0X_OK);
}

TEST(test_init_wrong_model_id)
{
    setup();
    model.hw.reset_values[VL_REG_MODEL_ID] = 0x00;
    hw_model_reset(&model.hw);

    vl53l0x_t dev;
    vl53l0x_err_t err = vl53l0x_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, VL53L0X_ERR_ID);
}

TEST(test_init_writes_sequence_config)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(model.hw.regs[VL_REG_SYSTEM_SEQUENCE_CONFIG], 0xFFu);
}

TEST(test_init_writes_msrc_config)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(model.hw.regs[VL_REG_MSRC_CONFIG_CONTROL], 0x12u);
}

TEST(test_init_writes_default_timeout)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);
    /* Default budget: 30000us -> timeout = 30000/2 = 15000 = 0x3A98 */
    ASSERT_EQ(model.hw.regs[VL_REG_TIMEOUT_HI], 0x3Au);
    ASSERT_EQ(model.hw.regs[VL_REG_TIMEOUT_LO], 0x98u);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_range_reasonable)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);

    uint16_t range_mm = 0;
    vl53l0x_err_t err = vl53l0x_read_range_mm(&dev, &range_mm);
    ASSERT_EQ(err, VL53L0X_OK);
    ASSERT_EQ(range_mm, 250);
}

TEST(test_read_range_out_of_range)
{
    setup();
    model.true_range_mm = 8190;  /* out-of-range sentinel */
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);

    uint16_t range_mm = 0;
    vl53l0x_err_t err = vl53l0x_read_range_mm(&dev, &range_mm);
    ASSERT_EQ(err, VL53L0X_ERR_RANGE);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_model_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, VL_REG_MODEL_ID, 0x00);
    ASSERT_EQ(model.hw.regs[VL_REG_MODEL_ID], 0xEEu);
}

TEST(test_range_data_readonly)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);
    /* Trigger measurement to populate registers */
    uint16_t range_mm;
    vl53l0x_read_range_mm(&dev, &range_mm);

    uint8_t orig = model.hw.regs[VL_REG_RANGE_DATA_HI];
    hw_model__write_reg(&model.hw, VL_REG_RANGE_DATA_HI, 0xFF);
    ASSERT_EQ(model.hw.regs[VL_REG_RANGE_DATA_HI], orig);
}

TEST(test_set_timing_budget)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);

    vl53l0x_err_t err = vl53l0x_set_measurement_timing_budget(&dev, 100000);
    ASSERT_EQ(err, VL53L0X_OK);
    /* 100000/2 = 50000 = 0xC350 (fits in uint16) */
    ASSERT_EQ(model.hw.regs[VL_REG_TIMEOUT_HI], 0xC3u);
    ASSERT_EQ(model.hw.regs[VL_REG_TIMEOUT_LO], 0x50u);
}

TEST(test_set_timing_budget_out_of_range)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);

    vl53l0x_err_t err = vl53l0x_set_measurement_timing_budget(&dev, 10000);
    ASSERT_EQ(err, VL53L0X_ERR_PARAM);
}

TEST(test_measure_count_increments)
{
    setup();
    vl53l0x_t dev;
    vl53l0x_init(&dev, &model_i2c, &default_cfg);

    int before = model.measure_count;
    uint16_t range_mm;
    vl53l0x_read_range_mm(&dev, &range_mm);
    ASSERT_EQ(model.measure_count, before + 1);
}

TEST_MAIN("vl53l0x hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_model_id);
    RUN(test_init_writes_sequence_config);
    RUN(test_init_writes_msrc_config);
    RUN(test_init_writes_default_timeout);
    RUN(test_read_range_reasonable);
    RUN(test_read_range_out_of_range);
    RUN(test_model_id_readonly);
    RUN(test_range_data_readonly);
    RUN(test_set_timing_budget);
    RUN(test_set_timing_budget_out_of_range);
    RUN(test_measure_count_increments);
TEST_MAIN_END()
