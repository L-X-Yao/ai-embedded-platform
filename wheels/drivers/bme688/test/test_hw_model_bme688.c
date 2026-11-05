// SPDX-License-Identifier: MIT
/*
 * test_hw_model_bme688.c — tests using hardware register model
 *
 * Validates BME688 driver against a datasheet-accurate register model.
 * The model enforces read-only registers, calibration parsing,
 * measurement triggers, gas resistance, and reset behavior.
 */

#include "test_harness.h"
#include "hw_model_bme688.h"
#include "../bme688.h"
#include <math.h>

static bme688_model_t model;

/* No-op delay for sim */
static void noop_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

static const bme688_i2c_t model_i2c = {
    .write_reg = hw_model_i2c_write_reg,
    .read_regs = hw_model_i2c_read_regs,
    .delay_ms  = noop_delay,
    .ctx       = &model.hw,
};

static const bme688_cfg_t hw_cfg = {
    .osrs_t = 1, .osrs_p = 1, .osrs_h = 1,
    .filter = 0,
    .gas_heater_temp_c = 0,
    .gas_heater_dur_ms = 0,
    .addr = 0x76,
};

static void setup(void)
{
    bme688_model_init(&model);
}

/* --- init tests -------------------------------------------------------- */

TEST(test_init_reads_chip_id)
{
    setup();
    bme688_t dev;
    bme688_err_t err = bme688_init(&dev, &model_i2c, &hw_cfg);
    ASSERT_EQ(err, BME688_OK);
}

TEST(test_init_wrong_chip_id)
{
    setup();
    model.hw.reset_values[BME688_REG_CHIP_ID] = 0xFF;
    hw_model_reset(&model.hw);
    bme688_model_set_data_regs(&model);

    bme688_t dev;
    bme688_err_t err = bme688_init(&dev, &model_i2c, &hw_cfg);
    ASSERT_EQ(err, BME688_ERR_ID);
}

TEST(test_init_reads_calibration)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);
    /* par_t1 at coeff2[8..9] = 25000 = 0x61A8 */
    ASSERT_EQ(dev.par_t1, 25000);
}

TEST(test_init_writes_ctrl_hum)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);
    /* Default oversampling 1x for humidity */
    ASSERT_EQ(model.hw.regs[BME688_REG_CTRL_HUM] & 0x07, 0x01);
}

/* --- read tests -------------------------------------------------------- */

TEST(test_read_triggers_forced_measurement)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);

    int count_before = model.measure_count;
    bme688_data_t data;
    bme688_err_t err = bme688_read(&dev, &data);
    ASSERT_EQ(err, BME688_OK);
    ASSERT_EQ(model.measure_count, count_before + 1);
}

TEST(test_read_temperature_reasonable)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);

    bme688_data_t data;
    bme688_err_t err = bme688_read(&dev, &data);
    ASSERT_EQ(err, BME688_OK);
    /* BME688 datasheet range: -40 to 85 C */
    ASSERT_TRUE(data.temperature_c >= -40.0f && data.temperature_c <= 85.0f);
}

TEST(test_read_pressure_reasonable)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);

    bme688_data_t data;
    bme688_read(&dev, &data);
    /* BME688 range: 300..1100 hPa = 30000..110000 Pa */
    ASSERT_TRUE(data.pressure_pa >= 30000.0f && data.pressure_pa <= 110000.0f);
}

TEST(test_read_humidity_reasonable)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);

    bme688_data_t data;
    bme688_read(&dev, &data);
    /* floating point form on manual p25 with the model's calibration,
     * hum_adc 20000 at 31.51 degC gives 28.01 %RH */
    ASSERT_NEAR(data.humidity_pct, 28.01f, 0.1f);
}

TEST(test_read_gas_nonnegative)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);
    ASSERT_EQ(bme688_set_gas_heater(&dev, 300, 100), BME688_OK);

    bme688_data_t data;
    bme688_read(&dev, &data);
    ASSERT_TRUE(data.gas_valid);
    ASSERT_TRUE(data.gas_resistance_ohm > 0);
}

/* The model holds gas_adc 500 in range 4. Manual p28 and p29:
 * 262144 >> 4 = 16384, 4096 + 3 * (500 - 512) = 4060,
 * 10000 * 16384 / 4060 = 40354, times 100 */
TEST(test_read_gas_resistance_from_manual)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);
    ASSERT_EQ(bme688_set_gas_heater(&dev, 300, 100), BME688_OK);

    bme688_data_t data;
    ASSERT_EQ(bme688_read(&dev, &data), BME688_OK);
    ASSERT_EQ(data.gas_resistance_ohm, 4035400);
}

/* Manual p27 with the model's calibration at 25 degC, 300 degC target:
 * the floating point form gives 110.9, the integer code gives 111 */
TEST(test_heater_code_from_manual)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);
    ASSERT_EQ(bme688_set_gas_heater(&dev, 300, 100), BME688_OK);
    ASSERT_EQ(model.hw.regs[BME688_REG_RES_HEAT0], 111);
}

/* Manual p40: gas conversions start only if run_gas = 1 */
TEST(test_read_no_gas_without_run_gas)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);

    bme688_data_t data;
    ASSERT_EQ(bme688_read(&dev, &data), BME688_OK);
    ASSERT_FALSE(data.gas_valid);
    ASSERT_EQ((long long)data.gas_resistance_ohm, 0);
}

/* --- datasheet behavior tests ------------------------------------------ */

TEST(test_chip_id_readonly)
{
    setup();
    hw_model__write_reg(&model.hw, BME688_REG_CHIP_ID, 0x00);
    ASSERT_EQ(model.hw.regs[BME688_REG_CHIP_ID], 0x61);
}

TEST(test_calib_registers_readonly)
{
    setup();
    uint8_t original = model.hw.regs[BME688_REG_COEFF1];
    hw_model__write_reg(&model.hw, BME688_REG_COEFF1, 0xFF);
    ASSERT_EQ(model.hw.regs[BME688_REG_COEFF1], original);
}

TEST(test_soft_reset_restores_defaults)
{
    setup();
    bme688_t dev;
    bme688_init(&dev, &model_i2c, &hw_cfg);

    /* Write something to a config register */
    hw_model__write_reg(&model.hw, BME688_REG_CTRL_GAS1, 0x10);
    ASSERT_EQ(model.hw.regs[BME688_REG_CTRL_GAS1], 0x10);

    bme688_reset(&dev);

    ASSERT_EQ(model.hw.regs[BME688_REG_CTRL_GAS1], 0x00);
    ASSERT_EQ(model.hw.regs[BME688_REG_CHIP_ID], 0x61);
}

TEST(test_data_registers_readonly)
{
    setup();
    bme688_model_set_data_regs(&model);
    uint8_t original = model.hw.regs[BME688_REG_DATA_START];
    hw_model__write_reg(&model.hw, BME688_REG_DATA_START, 0xFF);
    ASSERT_EQ(model.hw.regs[BME688_REG_DATA_START], original);
}

TEST_MAIN("bme688 hardware model")
    RUN(test_init_reads_chip_id);
    RUN(test_init_wrong_chip_id);
    RUN(test_init_reads_calibration);
    RUN(test_init_writes_ctrl_hum);
    RUN(test_read_triggers_forced_measurement);
    RUN(test_read_temperature_reasonable);
    RUN(test_read_pressure_reasonable);
    RUN(test_read_humidity_reasonable);
    RUN(test_read_gas_nonnegative);
    RUN(test_read_gas_resistance_from_manual);
    RUN(test_heater_code_from_manual);
    RUN(test_read_no_gas_without_run_gas);
    RUN(test_chip_id_readonly);
    RUN(test_calib_registers_readonly);
    RUN(test_soft_reset_restores_defaults);
    RUN(test_data_registers_readonly);
TEST_MAIN_END()
