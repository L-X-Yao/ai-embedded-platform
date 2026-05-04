/*
 * test_hw_model_bmp280.c — tests using hardware register model
 *
 * Validates BMP280 driver against a datasheet-accurate register model,
 * not just a mock. The model enforces read-only registers, reset
 * behavior, measurement triggers, and ADC output layout.
 */

#include "test_harness.h"
#include "hw_model_bmp280.h"
#include "../bmp280.h"
#include <math.h>

static bmp280_model_t model;

static const bmp280_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const bmp280_cfg_t default_cfg = {
    .addr   = BMP280_ADDR_LOW,
    .osrs_t = BMP280_OVERSAMPLING_1,
    .osrs_p = BMP280_OVERSAMPLING_1,
};

static void setup(void)
{
    bmp280_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_reads_chip_id)
{
    setup();
    bmp280_t dev;
    bmp280_err_t err = bmp280_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BMP280_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    /* Corrupt chip ID (model allows changing reset value before reset) */
    model.hw.reset_values[BMP280_REG_ID] = 0xFF;
    hw_model_reset(&model.hw);

    bmp280_t dev;
    bmp280_err_t err = bmp280_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BMP280_ERR_ID);
}

TEST(test_init_reads_calibration)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);
    /* dig_T1 = 27504 = 0x6B70 (little-endian at 0x88-0x89) */
    ASSERT_EQ(dev.calib.dig_t1, 27504);
    ASSERT_EQ(dev.calib.dig_t2, 26435);
}

TEST(test_init_writes_config_register)
{
    setup();
    model.hw.regs[BMP280_REG_CONFIG] = 0xFF;
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);
    /* Driver should write config register during init */
    ASSERT_EQ(model.hw.regs[BMP280_REG_CONFIG], 0x00);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_triggers_forced_measurement)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);

    int count_before = model.measure_count;
    bmp280_data_t data;
    bmp280_err_t err = bmp280_read(&dev, &data);
    ASSERT_EQ(err, BMP280_OK);
    ASSERT_EQ(model.measure_count, count_before + 1);
}

TEST(test_read_writes_correct_ctrl_meas)
{
    setup();
    bmp280_t dev;
    bmp280_cfg_t cfg = { .addr = BMP280_ADDR_LOW, .osrs_t = BMP280_OVERSAMPLING_4, .osrs_p = BMP280_OVERSAMPLING_2 };
    bmp280_init(&dev, &model_i2c, &cfg);

    bmp280_data_t data;
    bmp280_read(&dev, &data);
    /* ctrl_meas: osrs_t[7:5]=3, osrs_p[4:2]=2, mode[1:0]=forced(01)
     * but after forced mode completes, mode bits revert to 00 */
    uint8_t expected_written = (3 << 5) | (2 << 2) | 0x01;
    /* After measurement, mode bits cleared by model → 0x6C | 0x00 */
    uint8_t current = model.hw.regs[BMP280_REG_CTRL_MEAS];
    ASSERT_EQ(current & 0xFC, expected_written & 0xFC);
}

TEST(test_read_temperature_reasonable)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);

    bmp280_data_t data;
    bmp280_err_t err = bmp280_read(&dev, &data);
    ASSERT_EQ(err, BMP280_OK);
    /* With reference calib and ADC ~519888, expect ~25°C */
    ASSERT_TRUE(data.temperature_c > 10.0f && data.temperature_c < 40.0f);
}

TEST(test_read_pressure_reasonable)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);

    bmp280_data_t data;
    bmp280_read(&dev, &data);
    /* With reference calib, expect ~80000-120000 Pa (reasonable atmosphere) */
    ASSERT_TRUE(data.pressure_pa > 50000.0f && data.pressure_pa < 150000.0f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_chip_id_readonly)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);
    /* Try to write chip ID register — model should prevent it */
    model.hw.regs[BMP280_REG_ID] = 0x58; /* should stay 0x58 */
    hw_model__write_reg(&model.hw, BMP280_REG_ID, 0x00);
    ASSERT_EQ(model.hw.regs[BMP280_REG_ID], 0x58);
}

TEST(test_calib_registers_readonly)
{
    setup();
    uint8_t original = model.hw.regs[BMP280_REG_CALIB00];
    hw_model__write_reg(&model.hw, BMP280_REG_CALIB00, 0xFF);
    ASSERT_EQ(model.hw.regs[BMP280_REG_CALIB00], original);
}

TEST(test_soft_reset_restores_defaults)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);

    /* Write some values */
    hw_model__write_reg(&model.hw, BMP280_REG_CONFIG, 0xA0);
    ASSERT_EQ(model.hw.regs[BMP280_REG_CONFIG], 0xA0);

    /* Soft reset via driver */
    bmp280_reset(&dev);

    /* Config should be back to reset value (0x00) */
    ASSERT_EQ(model.hw.regs[BMP280_REG_CONFIG], 0x00);
    /* Chip ID still valid */
    ASSERT_EQ(model.hw.regs[BMP280_REG_ID], 0x58);
}

TEST(test_adc_registers_readonly)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);

    /* Trigger a measurement to populate ADC registers */
    bmp280_data_t data;
    bmp280_read(&dev, &data);

    uint8_t press_msb = model.hw.regs[BMP280_REG_PRESS_MSB];
    /* Try to overwrite — should be blocked */
    hw_model__write_reg(&model.hw, BMP280_REG_PRESS_MSB, 0xFF);
    ASSERT_EQ(model.hw.regs[BMP280_REG_PRESS_MSB], press_msb);
}

/* --- varying temperature ------------------------------------------------ */

TEST(test_different_temperatures)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);

    /* Cold: low ADC */
    bmp280_model_set_temperature(&model, 400000);
    bmp280_data_t data;
    bmp280_read(&dev, &data);
    float cold = data.temperature_c;

    /* Hot: high ADC */
    bmp280_model_set_temperature(&model, 600000);
    bmp280_read(&dev, &data);
    float hot = data.temperature_c;

    /* Higher ADC raw should produce higher temperature */
    ASSERT_TRUE(hot > cold);
}

TEST(test_multiple_reads_independent)
{
    setup();
    bmp280_t dev;
    bmp280_init(&dev, &model_i2c, &default_cfg);

    bmp280_data_t d1, d2;
    bmp280_read(&dev, &d1);
    bmp280_read(&dev, &d2);
    /* Same environment → same results */
    ASSERT_NEAR(d1.temperature_c, d2.temperature_c, 0.01f);
    ASSERT_NEAR(d1.pressure_pa, d2.pressure_pa, 1.0f);
    ASSERT_EQ(model.measure_count, 2);
}

TEST_MAIN("bmp280 hardware model")
    RUN(test_init_reads_chip_id);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_reads_calibration);
    RUN(test_init_writes_config_register);
    RUN(test_read_triggers_forced_measurement);
    RUN(test_read_writes_correct_ctrl_meas);
    RUN(test_read_temperature_reasonable);
    RUN(test_read_pressure_reasonable);
    RUN(test_chip_id_readonly);
    RUN(test_calib_registers_readonly);
    RUN(test_soft_reset_restores_defaults);
    RUN(test_adc_registers_readonly);
    RUN(test_different_temperatures);
    RUN(test_multiple_reads_independent);
TEST_MAIN_END()
