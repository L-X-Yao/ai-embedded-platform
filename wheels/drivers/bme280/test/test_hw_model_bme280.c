// SPDX-License-Identifier: MIT
/*
 * test_hw_model_bme280.c — tests using hardware register model
 *
 * Validates BME280 driver against a datasheet-accurate register model.
 * The model enforces read-only registers, reset behavior, measurement
 * triggers, humidity calibration, and ADC output layout.
 */

#include "test_harness.h"
#include "hw_model_bme280.h"
#include "../bme280.h"
#include <math.h>

static bme280_model_t model;

static const bme280_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = NULL,
    .ctx       = &model.hw,
};

static const bme280_cfg_t default_cfg = {
    .addr   = BME280_ADDR_LOW,
    .osrs_t = BME280_OVERSAMPLING_1,
    .osrs_p = BME280_OVERSAMPLING_1,
    .osrs_h = BME280_OVERSAMPLING_1,
};

static void setup(void)
{
    bme280_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_reads_chip_id)
{
    setup();
    bme280_t dev;
    bme280_err_t err = bme280_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BME280_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[BME280_REG_ID] = 0xFF;
    hw_model_reset(&model.hw);

    bme280_t dev;
    bme280_err_t err = bme280_init(&dev, &model_i2c, &default_cfg);
    ASSERT_EQ(err, BME280_ERR_ID);
}

TEST(test_init_reads_temp_calibration)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);
    /* dig_T1 = 27504 = 0x6B70 (little-endian at 0x88-0x89) */
    ASSERT_EQ(dev.calib.dig_t1, 27504);
    ASSERT_EQ(dev.calib.dig_t2, 26435);
}

TEST(test_init_reads_humidity_calibration)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);
    /* dig_H1 at 0xA1 = 0x4B = 75 */
    ASSERT_EQ(dev.calib.dig_h1, 75);
    /* dig_H2 at 0xE1..0xE2 = 0x016E = 366 (little-endian) */
    ASSERT_EQ(dev.calib.dig_h2, 366);
}

TEST(test_init_writes_ctrl_hum)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);
    /* ctrl_hum should be written with osrs_h = 1 */
    ASSERT_EQ(model.hw.regs[BME280_REG_CTRL_HUM] & 0x07, 1);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_triggers_forced_measurement)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);

    int count_before = model.measure_count;
    bme280_data_t data;
    bme280_err_t err = bme280_read(&dev, &data);
    ASSERT_EQ(err, BME280_OK);
    ASSERT_EQ(model.measure_count, count_before + 1);
}

TEST(test_read_temperature_reasonable)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);

    bme280_data_t data;
    bme280_err_t err = bme280_read(&dev, &data);
    ASSERT_EQ(err, BME280_OK);
    /* With reference calib and ADC ~519888, expect ~25C */
    ASSERT_TRUE(data.temperature_c > 10.0f && data.temperature_c < 40.0f);
}

TEST(test_read_pressure_reasonable)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);

    bme280_data_t data;
    bme280_read(&dev, &data);
    ASSERT_TRUE(data.pressure_pa > 50000.0f && data.pressure_pa < 150000.0f);
}

TEST(test_read_humidity_nonnegative)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);

    bme280_data_t data;
    bme280_read(&dev, &data);
    ASSERT_TRUE(data.humidity_pct >= 0.0f && data.humidity_pct <= 100.0f);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, BME280_REG_ID, 0x00);
    ASSERT_EQ(model.hw.regs[BME280_REG_ID], 0x60);
}

TEST(test_calib_registers_readonly)
{
    setup();
    uint8_t original = model.hw.regs[BME280_REG_CALIB00];
    hw_model__write_reg(&model.hw, BME280_REG_CALIB00, 0xFF);
    ASSERT_EQ(model.hw.regs[BME280_REG_CALIB00], original);
}

TEST(test_soft_reset_restores_defaults)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);

    hw_model__write_reg(&model.hw, BME280_REG_CONFIG, 0xA0);
    ASSERT_EQ(model.hw.regs[BME280_REG_CONFIG], 0xA0);

    bme280_reset(&dev);

    ASSERT_EQ(model.hw.regs[BME280_REG_CONFIG], 0x00);
    ASSERT_EQ(model.hw.regs[BME280_REG_ID], 0x60);
}

TEST(test_adc_registers_readonly)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);

    bme280_data_t data;
    bme280_read(&dev, &data);

    uint8_t press_msb = model.hw.regs[BME280_REG_PRESS_MSB];
    hw_model__write_reg(&model.hw, BME280_REG_PRESS_MSB, 0xFF);
    ASSERT_EQ(model.hw.regs[BME280_REG_PRESS_MSB], press_msb);
}

TEST(test_humidity_calib_readonly)
{
    setup();
    uint8_t original = model.hw.regs[BME280_REG_CALIB_H2];
    hw_model__write_reg(&model.hw, BME280_REG_CALIB_H2, 0xFF);
    ASSERT_EQ(model.hw.regs[BME280_REG_CALIB_H2], original);
}

TEST(test_different_temperatures)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);

    bme280_model_set_temperature(&model, 400000);
    bme280_data_t data;
    bme280_read(&dev, &data);
    float cold = data.temperature_c;

    bme280_model_set_temperature(&model, 600000);
    bme280_read(&dev, &data);
    float hot = data.temperature_c;

    ASSERT_TRUE(hot > cold);
}

TEST(test_multiple_reads_independent)
{
    setup();
    bme280_t dev;
    bme280_init(&dev, &model_i2c, &default_cfg);

    bme280_data_t d1, d2;
    bme280_read(&dev, &d1);
    bme280_read(&dev, &d2);
    ASSERT_NEAR(d1.temperature_c, d2.temperature_c, 0.01f);
    ASSERT_NEAR(d1.pressure_pa, d2.pressure_pa, 1.0f);
    ASSERT_EQ(model.measure_count, 2);
}

/* --- skipped channels (manual Tables 20, 23, 24) ----------------------- */

/* Worked by hand from the manual's code with the model's calibration: adc_T
 * 519888 gives t_fine 128422 and 25.08 C, adc_H 28000 then gives 52.74 %RH
 * (section 4.2.3), and adc_P 415148 gives 100653.26 Pa (section 8.1). */
#define REF_ADC_T    519888
#define REF_ADC_P    415148
#define REF_ADC_H    28000
#define REF_TEMP_C   25.08f
#define REF_PRESS_PA 100653.26f
#define REF_HUM_PCT  52.74f

static void setup_reference(void)
{
    setup();
    bme280_model_set_temperature(&model, REF_ADC_T);
    bme280_model_set_pressure(&model, REF_ADC_P);
    bme280_model_set_humidity(&model, REF_ADC_H);
}

static bme280_cfg_t cfg_of(bme280_oversampling_t osrs_t,
                           bme280_oversampling_t osrs_p,
                           bme280_oversampling_t osrs_h)
{
    bme280_cfg_t cfg = { BME280_ADDR_LOW, osrs_t, osrs_p, osrs_h };
    return cfg;
}

TEST(test_model_skip_outputs_placeholders)
{
    setup_reference();
    hw_model__write_reg(&model.hw, BME280_REG_CTRL_HUM, 0x00);
    hw_model__write_reg(&model.hw, BME280_REG_CTRL_MEAS, 0x01);

    ASSERT_EQ(model.hw.regs[0xF7], 0x80);
    ASSERT_EQ(model.hw.regs[0xF8], 0x00);
    ASSERT_EQ(model.hw.regs[0xF9], 0x00);
    ASSERT_EQ(model.hw.regs[0xFA], 0x80);
    ASSERT_EQ(model.hw.regs[0xFB], 0x00);
    ASSERT_EQ(model.hw.regs[0xFC], 0x00);
    ASSERT_EQ(model.hw.regs[0xFD], 0x80);
    ASSERT_EQ(model.hw.regs[0xFE], 0x00);
}

TEST(test_model_ctrl_hum_waits_for_ctrl_meas)
{
    setup_reference();
    hw_model__write_reg(&model.hw, BME280_REG_CTRL_HUM, 0x00);
    hw_model__write_reg(&model.hw, BME280_REG_CTRL_MEAS, 0x25);
    ASSERT_EQ(model.hw.regs[0xFD], 0x80);

    hw_model__write_reg(&model.hw, BME280_REG_CTRL_HUM, 0x01);
    ASSERT_EQ(model.hw.regs[0xFD], 0x80);

    hw_model__write_reg(&model.hw, BME280_REG_CTRL_MEAS, 0x25);
    ASSERT_EQ(model.hw.regs[0xFD], REF_ADC_H >> 8);
}

TEST(test_all_channels_match_the_manual)
{
    setup_reference();
    bme280_t dev;
    ASSERT_EQ(bme280_init(&dev, &model_i2c, &default_cfg), BME280_OK);

    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.pressure_pa, REF_PRESS_PA, 0.05f);
    ASSERT_NEAR(data.humidity_pct, REF_HUM_PCT, 0.01f);
}

TEST(test_skip_pressure_reads_zero)
{
    setup_reference();
    bme280_cfg_t cfg = cfg_of(BME280_OVERSAMPLING_1, BME280_OVERSAMPLING_SKIP,
                              BME280_OVERSAMPLING_1);
    bme280_t dev;
    ASSERT_EQ(bme280_init(&dev, &model_i2c, &cfg), BME280_OK);

    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);
    ASSERT_NEAR(data.pressure_pa, 0.0f, 0.001f);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.humidity_pct, REF_HUM_PCT, 0.01f);
}

TEST(test_skip_humidity_reads_zero)
{
    setup_reference();
    bme280_cfg_t cfg = cfg_of(BME280_OVERSAMPLING_1, BME280_OVERSAMPLING_1,
                              BME280_OVERSAMPLING_SKIP);
    bme280_t dev;
    ASSERT_EQ(bme280_init(&dev, &model_i2c, &cfg), BME280_OK);

    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);
    ASSERT_NEAR(data.humidity_pct, 0.0f, 0.001f);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.pressure_pa, REF_PRESS_PA, 0.05f);
}

TEST(test_skip_temperature_measures_once_in_init)
{
    setup_reference();
    bme280_cfg_t cfg = cfg_of(BME280_OVERSAMPLING_SKIP, BME280_OVERSAMPLING_2,
                              BME280_OVERSAMPLING_4);
    bme280_t dev;
    ASSERT_EQ(bme280_init(&dev, &model_i2c, &cfg), BME280_OK);

    ASSERT_EQ(model.measure_count, 1);
    /* temperature alone at x1: osrs_t 001, osrs_p 000, mode back at sleep */
    ASSERT_EQ(model.hw.regs[BME280_REG_CTRL_MEAS], 0x20);
    /* init still leaves ctrl_hum at the configured value */
    ASSERT_EQ(model.hw.regs[BME280_REG_CTRL_HUM], BME280_OVERSAMPLING_4);
}

TEST(test_skip_temperature_reuses_the_one_from_init)
{
    setup_reference();
    bme280_cfg_t cfg = cfg_of(BME280_OVERSAMPLING_SKIP, BME280_OVERSAMPLING_1,
                              BME280_OVERSAMPLING_1);
    bme280_t dev;
    ASSERT_EQ(bme280_init(&dev, &model_i2c, &cfg), BME280_OK);

    /* A later change cannot reach the driver: temperature is not measured */
    bme280_model_set_temperature(&model, 400000);

    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.pressure_pa, REF_PRESS_PA, 0.05f);
    ASSERT_NEAR(data.humidity_pct, REF_HUM_PCT, 0.01f);

    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_EQ(model.measure_count, 3);   /* init, then one per read */
}

TEST(test_skip_temperature_after_a_read_keeps_it)
{
    setup_reference();
    bme280_t dev;
    ASSERT_EQ(bme280_init(&dev, &model_i2c, &default_cfg), BME280_OK);
    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);

    ASSERT_EQ(bme280_set_oversampling(&dev, BME280_OVERSAMPLING_SKIP,
                                      BME280_OVERSAMPLING_1,
                                      BME280_OVERSAMPLING_1), BME280_OK);
    bme280_model_set_temperature(&model, 400000);

    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.pressure_pa, REF_PRESS_PA, 0.05f);
    ASSERT_NEAR(data.humidity_pct, REF_HUM_PCT, 0.01f);
    ASSERT_EQ(model.measure_count, 2);
}

TEST(test_skip_temperature_before_any_read_measures_it)
{
    setup_reference();
    bme280_t dev;
    ASSERT_EQ(bme280_init(&dev, &model_i2c, &default_cfg), BME280_OK);
    ASSERT_EQ(bme280_set_oversampling(&dev, BME280_OVERSAMPLING_SKIP,
                                      BME280_OVERSAMPLING_1,
                                      BME280_OVERSAMPLING_1), BME280_OK);

    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.pressure_pa, REF_PRESS_PA, 0.05f);
    ASSERT_NEAR(data.humidity_pct, REF_HUM_PCT, 0.01f);
    ASSERT_EQ(model.measure_count, 2);   /* the temperature, then the read */

    ASSERT_EQ(bme280_read(&dev, &data), BME280_OK);
    ASSERT_EQ(model.measure_count, 3);
}

TEST(test_skip_all_refused)
{
    setup();
    bme280_cfg_t cfg = cfg_of(BME280_OVERSAMPLING_SKIP, BME280_OVERSAMPLING_SKIP,
                              BME280_OVERSAMPLING_SKIP);
    bme280_t dev;
    ASSERT_EQ(bme280_init(&dev, &model_i2c, &cfg), BME280_ERR_PARAM);

    ASSERT_EQ(bme280_init(&dev, &model_i2c, &default_cfg), BME280_OK);
    ASSERT_EQ(bme280_set_oversampling(&dev, BME280_OVERSAMPLING_SKIP,
                                      BME280_OVERSAMPLING_SKIP,
                                      BME280_OVERSAMPLING_SKIP), BME280_ERR_PARAM);
    ASSERT_EQ(dev.cfg.osrs_t, BME280_OVERSAMPLING_1);
}

TEST_MAIN("bme280 hardware model")
    RUN(test_init_reads_chip_id);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_reads_temp_calibration);
    RUN(test_init_reads_humidity_calibration);
    RUN(test_init_writes_ctrl_hum);
    RUN(test_read_triggers_forced_measurement);
    RUN(test_read_temperature_reasonable);
    RUN(test_read_pressure_reasonable);
    RUN(test_read_humidity_nonnegative);
    RUN(test_chip_id_readonly);
    RUN(test_calib_registers_readonly);
    RUN(test_soft_reset_restores_defaults);
    RUN(test_adc_registers_readonly);
    RUN(test_humidity_calib_readonly);
    RUN(test_different_temperatures);
    RUN(test_multiple_reads_independent);
    RUN(test_model_skip_outputs_placeholders);
    RUN(test_model_ctrl_hum_waits_for_ctrl_meas);
    RUN(test_all_channels_match_the_manual);
    RUN(test_skip_pressure_reads_zero);
    RUN(test_skip_humidity_reads_zero);
    RUN(test_skip_temperature_measures_once_in_init);
    RUN(test_skip_temperature_reuses_the_one_from_init);
    RUN(test_skip_temperature_after_a_read_keeps_it);
    RUN(test_skip_temperature_before_any_read_measures_it);
    RUN(test_skip_all_refused);
TEST_MAIN_END()
