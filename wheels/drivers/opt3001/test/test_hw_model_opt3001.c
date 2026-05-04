/*
 * test_hw_model_opt3001.c — tests using hardware register model
 *
 * Validates OPT3001 driver against a datasheet-accurate register model.
 * The model enforces read-only manufacturer/device IDs, 16-bit register
 * protocol, lux encoding, config register mode bits, and CRF flag.
 */

#include "test_harness.h"
#include "hw_model_opt3001.h"
#include "../opt3001.h"
#include <math.h>

static opt3001_model_t model;

static const opt3001_i2c_t model_i2c = {
    .write_regs = opt3001_model_i2c_write_regs,
    .read_regs  = opt3001_model_i2c_read_regs,
    .delay_ms   = NULL,
    .ctx        = &model,
};

static void setup(void)
{
    opt3001_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_ok)
{
    setup();
    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    ASSERT_EQ(err, OPT3001_OK);
}

TEST(test_init_wrong_manufacturer_id)
{
    setup();
    /* Corrupt manufacturer ID */
    uint8_t mfr_base = OPT_BYTE_ADDR(OPT_REG_MANUFACTURER_ID);
    model.hw.reset_values[mfr_base] = 0x00;
    model.hw.reset_values[mfr_base + 1] = 0x00;
    hw_model_reset(&model.hw);

    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    ASSERT_EQ(err, OPT3001_ERR_ID);
}

TEST(test_init_wrong_device_id)
{
    setup();
    /* Corrupt device ID */
    uint8_t dev_base = OPT_BYTE_ADDR(OPT_REG_DEVICE_ID);
    model.hw.reset_values[dev_base] = 0xFF;
    model.hw.reset_values[dev_base + 1] = 0xFF;
    hw_model_reset(&model.hw);

    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    ASSERT_EQ(err, OPT3001_ERR_ID);
}

TEST(test_init_writes_default_config)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    /* Default config 0xC010 should be written */
    uint8_t cfg_base = OPT_BYTE_ADDR(OPT_REG_CONFIG);
    ASSERT_EQ(model.hw.regs[cfg_base], 0xC0u);
    ASSERT_EQ(model.hw.regs[cfg_base + 1], 0x10u);
}

TEST(test_init_invalid_address)
{
    setup();
    opt3001_t dev;
    opt3001_err_t err = opt3001_init(&dev, &model_i2c, 0x30);
    ASSERT_EQ(err, OPT3001_ERR_PARAM);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_lux_reasonable)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);

    /* Trigger single-shot to populate result */
    opt3001_start_single(&dev);

    float lux = 0.0f;
    opt3001_err_t err = opt3001_read_lux(&dev, &lux);
    ASSERT_EQ(err, OPT3001_OK);
    /* With true_lux=500, encoding and decoding should be close */
    ASSERT_TRUE(lux > 400.0f && lux < 600.0f);
}

TEST(test_data_ready_after_single)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    opt3001_start_single(&dev);
    ASSERT_TRUE(opt3001_data_ready(&dev));
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_manufacturer_id_readonly)
{
    setup();
    uint8_t mfr_base = OPT_BYTE_ADDR(OPT_REG_MANUFACTURER_ID);
    hw_model__write_reg(&model.hw, mfr_base, 0xFF);
    ASSERT_EQ(model.hw.regs[mfr_base], 0x54u);
}

TEST(test_device_id_readonly)
{
    setup();
    uint8_t dev_base = OPT_BYTE_ADDR(OPT_REG_DEVICE_ID);
    hw_model__write_reg(&model.hw, dev_base, 0xFF);
    ASSERT_EQ(model.hw.regs[dev_base], 0x30u);
}

TEST(test_result_register_readonly)
{
    setup();
    uint8_t res_base = OPT_BYTE_ADDR(OPT_REG_RESULT);
    uint8_t orig = model.hw.regs[res_base];
    hw_model__write_reg(&model.hw, res_base, 0xFF);
    ASSERT_EQ(model.hw.regs[res_base], orig);
}

TEST(test_continuous_mode)
{
    setup();
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    opt3001_set_continuous(&dev);

    float lux = 0.0f;
    opt3001_err_t err = opt3001_read_lux(&dev, &lux);
    ASSERT_EQ(err, OPT3001_OK);
    ASSERT_TRUE(lux > 0.0f);
}

TEST(test_different_light_levels)
{
    setup();
    model.true_lux = 100.0f;
    opt3001_t dev;
    opt3001_init(&dev, &model_i2c, OPT3001_ADDR_GND_GND);
    opt3001_start_single(&dev);
    float low_lux = 0.0f;
    opt3001_read_lux(&dev, &low_lux);

    model.true_lux = 10000.0f;
    opt3001_start_single(&dev);
    float high_lux = 0.0f;
    opt3001_read_lux(&dev, &high_lux);

    ASSERT_TRUE(high_lux > low_lux);
}

TEST_MAIN("opt3001 hardware model")
    RUN(test_init_ok);
    RUN(test_init_wrong_manufacturer_id);
    RUN(test_init_wrong_device_id);
    RUN(test_init_writes_default_config);
    RUN(test_init_invalid_address);
    RUN(test_read_lux_reasonable);
    RUN(test_data_ready_after_single);
    RUN(test_manufacturer_id_readonly);
    RUN(test_device_id_readonly);
    RUN(test_result_register_readonly);
    RUN(test_continuous_mode);
    RUN(test_different_light_levels);
TEST_MAIN_END()
