// SPDX-License-Identifier: MIT
/*
 * test_hw_model_bmp280.c — tests using hardware register model
 *
 * Validates BMP280 driver against a datasheet-accurate register model,
 * not just a mock. The model enforces read-only registers, reset
 * behavior, measurement triggers, ADC output layout, and the time a
 * start-up and a conversion take.
 */

#include "test_harness.h"
#include "hw_model_bmp280.h"
#include "../bmp280.h"
#include <math.h>

static bmp280_model_t model;

static const bmp280_i2c_t model_i2c = {
    .write_reg = bmp280_model_write_reg,
    .read_regs = bmp280_model_read_regs,
    .delay_ms  = bmp280_model_delay,
    .ctx       = &model.hw,
};

/* delay_ms is optional: only the time of the bus reads then passes. */
static const bmp280_i2c_t no_delay_i2c = {
    .write_reg = bmp280_model_write_reg,
    .read_regs = bmp280_model_read_regs,
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

/* Past tstartup and the NVM copy, as a caller with no delay_ms must wait. */
static void pass_start_up(void)
{
    bmp280_model_advance_us(&model, 2000u + BMP280_MODEL_NVM_COPY_US);
}

/* Longer than any conversion: x16 on both channels is 75.4 ms at the most. */
static void finish_conversion(void)
{
    bmp280_model_advance_us(&model, 80000u);
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

/* --- skipped channels (manual Tables 21, 22) --------------------------- */

/* Worked by hand from the manual's section 8.2 code with the model's
 * calibration: adc_T 519888 gives t_fine 128422 and 25.08 C, and adc_P 415148
 * then gives 100656 Pa. */
#define REF_ADC_T    519888
#define REF_ADC_P    415148
#define REF_TEMP_C   25.08f
#define REF_PRESS_PA 100656.0f

static void setup_reference(void)
{
    setup();
    bmp280_model_set_temperature(&model, REF_ADC_T);
    bmp280_model_set_pressure(&model, REF_ADC_P);
}

TEST(test_model_skip_outputs_placeholders)
{
    setup_reference();
    hw_model__write_reg(&model.hw, BMP280_REG_CTRL_MEAS, 0x01);
    finish_conversion();

    ASSERT_EQ(model.hw.regs[0xF7], 0x80);
    ASSERT_EQ(model.hw.regs[0xF8], 0x00);
    ASSERT_EQ(model.hw.regs[0xF9], 0x00);
    ASSERT_EQ(model.hw.regs[0xFA], 0x80);
    ASSERT_EQ(model.hw.regs[0xFB], 0x00);
    ASSERT_EQ(model.hw.regs[0xFC], 0x00);
}

/* Table 18 p.24: the reset state of the data and control registers. */
TEST(test_model_reset_state)
{
    setup();
    ASSERT_EQ_HEX(model.hw.regs[0xF7], 0x80);
    ASSERT_EQ_HEX(model.hw.regs[0xF8], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[0xF9], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[0xFA], 0x80);
    ASSERT_EQ_HEX(model.hw.regs[0xFB], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[0xFC], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_CTRL_MEAS], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_CONFIG], 0x00);
}

/* Table 10 p.15: "01 and 10 Forced mode" */
TEST(test_model_mode_10_is_forced)
{
    setup_reference();
    hw_model__write_reg(&model.hw, BMP280_REG_CTRL_MEAS, 0x26);
    ASSERT_EQ(model.measure_count, 1);

    finish_conversion();
    ASSERT_EQ_HEX(model.hw.regs[0xFA], REF_ADC_T >> 12);
    ASSERT_EQ(model.hw.regs[BMP280_REG_CTRL_MEAS] & 0x03, 0);
}

/* p.24: "Writing other values than 0xB6 has no effect. The readout value is
 * always 0x00." */
TEST(test_model_reset_register_reads_zero)
{
    setup();
    hw_model__write_reg(&model.hw, BMP280_REG_CONFIG, 0xA0);

    hw_model__write_reg(&model.hw, BMP280_REG_RESET, 0x55);
    ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_RESET], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_CONFIG], 0xA0);

    hw_model__write_reg(&model.hw, BMP280_REG_RESET, 0xB6);
    ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_RESET], 0x00);
    ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_CONFIG], 0x00);
}

/* Table 13 p.18, Max, in ms. Each row is printed to one decimal. */
TEST(test_model_conversion_takes_the_table_13_time)
{
    static const struct {
        uint8_t  osrs_p;
        uint8_t  osrs_t;
        uint32_t max_us;
    } rows[] = {
        { 1, 1,  6400 },   /* x1,  x1 */
        { 2, 1,  8700 },   /* x2,  x1 */
        { 3, 1, 13300 },   /* x4,  x1 */
        { 4, 1, 22500 },   /* x8,  x1 */
        { 5, 2, 43200 },   /* x16, x2 */
    };

    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        setup_reference();
        hw_model__write_reg(&model.hw, BMP280_REG_CTRL_MEAS,
                            (uint8_t)(rows[i].osrs_t << 5 | rows[i].osrs_p << 2 | 1));

        bmp280_model_advance_us(&model, rows[i].max_us);
        ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_STATUS] & 0x08, 0x08);
        ASSERT_EQ_HEX(model.hw.regs[0xFA], 0x80);
        ASSERT_EQ(model.hw.regs[BMP280_REG_CTRL_MEAS] & 0x03, 1);

        bmp280_model_advance_us(&model, 100u);
        ASSERT_EQ(model.hw.regs[BMP280_REG_STATUS] & 0x08, 0);
        ASSERT_EQ_HEX(model.hw.regs[0xFA], REF_ADC_T >> 12);
        ASSERT_EQ(model.hw.regs[BMP280_REG_CTRL_MEAS] & 0x03, 0);
    }
}

/* Table 19 p.25: the NVM data "are copied at power-on-reset and before every
 * conversion", so measuring is not yet 1 right after the trigger. */
TEST(test_model_nvm_copy_comes_before_the_conversion)
{
    setup_reference();
    hw_model__write_reg(&model.hw, BMP280_REG_CTRL_MEAS, 0x25);
    ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_STATUS], 0x01);

    bmp280_model_advance_us(&model, BMP280_MODEL_NVM_COPY_US);
    ASSERT_EQ_HEX(model.hw.regs[BMP280_REG_STATUS], 0x08);
}

/* A status read at 400 kHz takes 38 clocks, 95 us, so polling alone uses time. */
TEST(test_model_bus_reads_take_time)
{
    setup_reference();
    pass_start_up();
    hw_model__write_reg(&model.hw, BMP280_REG_CTRL_MEAS, 0x25);

    uint8_t status = 0;
    /* osrs_t x1, osrs_p x1: 6.425 ms, 68 reads */
    for (int i = 0; i < 67; i++)
        ASSERT_TRUE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                           BMP280_REG_STATUS, &status, 1));
    ASSERT_EQ_HEX(status & 0x08, 0x08);
    ASSERT_TRUE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                       BMP280_REG_STATUS, &status, 1));
    ASSERT_TRUE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                       BMP280_REG_STATUS, &status, 1));
    ASSERT_EQ(status & 0x08, 0);
}

/* p.8: tstartup, "Time to first communication", is 2 ms at most. */
TEST(test_model_no_access_during_start_up)
{
    setup();
    uint8_t id = 0;
    ASSERT_FALSE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                        BMP280_REG_ID, &id, 1));
    ASSERT_FALSE(bmp280_model_write_reg(&model.hw, BMP280_ADDR_LOW,
                                        BMP280_REG_CONFIG, 0x00));
    bmp280_model_advance_us(&model, 1800u);
    ASSERT_FALSE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                        BMP280_REG_ID, &id, 1));
    bmp280_model_advance_us(&model, 200u);
    ASSERT_TRUE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                       BMP280_REG_ID, &id, 1));
    ASSERT_EQ_HEX(id, 0x58);
}

/* p.24: 0xB6 runs "the complete power-on-reset procedure". */
TEST(test_model_no_access_after_soft_reset)
{
    setup();
    pass_start_up();
    ASSERT_TRUE(bmp280_model_write_reg(&model.hw, BMP280_ADDR_LOW,
                                       BMP280_REG_RESET, 0xB6));
    uint8_t id = 0;
    ASSERT_FALSE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                        BMP280_REG_ID, &id, 1));
    bmp280_model_advance_us(&model, 2000u);
    ASSERT_TRUE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                       BMP280_REG_ID, &id, 1));
}

/* Table 19 p.25: im_update is 1 while the NVM data are being copied to the
 * image registers, so calibration read then is not the calibration. */
TEST(test_model_calibration_is_not_ready_during_the_nvm_copy)
{
    setup();
    bmp280_model_advance_us(&model, 2000u);

    uint8_t status = 0;
    uint8_t dig_t1[2] = { 0xA5, 0xA5 };
    ASSERT_TRUE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                       BMP280_REG_STATUS, &status, 1));
    ASSERT_EQ_HEX(status & 0x01, 0x01);
    ASSERT_TRUE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                       BMP280_REG_CALIB00, dig_t1, 2));
    ASSERT_EQ_HEX(dig_t1[0], 0x00);

    bmp280_model_advance_us(&model, BMP280_MODEL_NVM_COPY_US);
    ASSERT_TRUE(bmp280_model_read_regs(&model.hw, BMP280_ADDR_LOW,
                                       BMP280_REG_CALIB00, dig_t1, 2));
    ASSERT_EQ_HEX(dig_t1[0], 0x70);
}

TEST(test_both_channels_match_the_manual)
{
    setup_reference();
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, &model_i2c, &default_cfg), BMP280_OK);

    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.pressure_pa, REF_PRESS_PA, 0.5f);
}

TEST(test_skip_pressure_reads_zero)
{
    setup_reference();
    bmp280_cfg_t cfg = { BMP280_ADDR_LOW, BMP280_OVERSAMPLING_1,
                         BMP280_OVERSAMPLING_SKIP };
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, &model_i2c, &cfg), BMP280_OK);
    ASSERT_EQ(model.measure_count, 0);

    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
    ASSERT_NEAR(data.pressure_pa, 0.0f, 0.001f);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
}

TEST(test_skip_temperature_measures_once_in_init)
{
    setup_reference();
    bmp280_cfg_t cfg = { BMP280_ADDR_LOW, BMP280_OVERSAMPLING_SKIP,
                         BMP280_OVERSAMPLING_2 };
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, &model_i2c, &cfg), BMP280_OK);

    ASSERT_EQ(model.measure_count, 1);
    /* temperature alone at x1: osrs_t 001, osrs_p 000, mode back at sleep */
    ASSERT_EQ(model.hw.regs[BMP280_REG_CTRL_MEAS], 0x20);
}

TEST(test_skip_temperature_reuses_the_one_from_init)
{
    setup_reference();
    bmp280_cfg_t cfg = { BMP280_ADDR_LOW, BMP280_OVERSAMPLING_SKIP,
                         BMP280_OVERSAMPLING_1 };
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, &model_i2c, &cfg), BMP280_OK);

    /* A later change cannot reach the driver: temperature is not measured */
    bmp280_model_set_temperature(&model, 400000);

    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.pressure_pa, REF_PRESS_PA, 0.5f);

    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_EQ(model.measure_count, 3);   /* init, then one per read */
}

/* --- the wait for a conversion ---------------------------------------- */

/* The sample read must be the one this call asked for. Table 19 p.25: measuring
 * is 0 during the NVM copy, so that bit alone can end the wait too early. */
static void check_read_is_new(const bmp280_i2c_t *i2c)
{
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, i2c, &default_cfg), BMP280_OK);

    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);

    bmp280_model_set_temperature(&model, 400000);
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
    ASSERT_TRUE(data.temperature_c < 10.0f);
}

TEST(test_read_gets_this_measurement)
{
    setup_reference();
    check_read_is_new(&model_i2c);
}

TEST(test_read_gets_this_measurement_with_no_delay)
{
    setup_reference();
    pass_start_up();
    check_read_is_new(&no_delay_i2c);
}

/* x16 on both channels: the driver accepts it, and the formula that matches
 * every row of Table 13 gives 75.4 ms for it. */
static void check_x16_read(const bmp280_i2c_t *i2c)
{
    bmp280_cfg_t cfg = { BMP280_ADDR_LOW, BMP280_OVERSAMPLING_16,
                         BMP280_OVERSAMPLING_16 };
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, i2c, &cfg), BMP280_OK);

    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
    ASSERT_NEAR(data.pressure_pa, REF_PRESS_PA, 0.5f);
}

TEST(test_read_at_x16_finishes)
{
    setup_reference();
    check_x16_read(&model_i2c);
}

TEST(test_read_at_x16_finishes_with_no_delay)
{
    setup_reference();
    pass_start_up();
    check_x16_read(&no_delay_i2c);
}

/* --- start-up and soft reset ------------------------------------------- */

/* p.8: first communication 2 ms after power-on, and the calibration only once
 * the NVM copy is over (Table 19 p.25). */
TEST(test_init_at_power_on_reads_the_calibration)
{
    setup();
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, &model_i2c, &default_cfg), BMP280_OK);
    ASSERT_EQ(dev.calib.dig_t1, 27504);
}

/* With no delay_ms the 2 ms are the caller's, as the header says. */
TEST(test_init_with_no_delay_after_the_caller_waited)
{
    setup();
    bmp280_model_advance_us(&model, 2000u);
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, &no_delay_i2c, &default_cfg), BMP280_OK);
    ASSERT_EQ(dev.calib.dig_t1, 27504);
}

static void check_read_after_reset(const bmp280_i2c_t *i2c)
{
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, i2c, &default_cfg), BMP280_OK);
    ASSERT_EQ(bmp280_reset(&dev), BMP280_OK);

    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
    ASSERT_NEAR(data.temperature_c, REF_TEMP_C, 0.001f);
}

TEST(test_read_right_after_reset)
{
    setup_reference();
    check_read_after_reset(&model_i2c);
}

TEST(test_read_right_after_reset_with_no_delay)
{
    setup_reference();
    pass_start_up();
    check_read_after_reset(&no_delay_i2c);
}

TEST(test_init_right_after_reset_reads_the_calibration)
{
    setup();
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, &model_i2c, &default_cfg), BMP280_OK);
    ASSERT_EQ(bmp280_reset(&dev), BMP280_OK);
    ASSERT_EQ(bmp280_init(&dev, &model_i2c, &default_cfg), BMP280_OK);
    ASSERT_EQ(dev.calib.dig_t1, 27504);
}

TEST(test_skip_both_refused)
{
    setup();
    bmp280_cfg_t cfg = { BMP280_ADDR_LOW, BMP280_OVERSAMPLING_SKIP,
                         BMP280_OVERSAMPLING_SKIP };
    bmp280_t dev;
    ASSERT_EQ(bmp280_init(&dev, &model_i2c, &cfg), BMP280_ERR_PARAM);
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
    RUN(test_model_skip_outputs_placeholders);
    RUN(test_model_reset_state);
    RUN(test_model_mode_10_is_forced);
    RUN(test_model_reset_register_reads_zero);
    RUN(test_model_conversion_takes_the_table_13_time);
    RUN(test_model_nvm_copy_comes_before_the_conversion);
    RUN(test_model_bus_reads_take_time);
    RUN(test_model_no_access_during_start_up);
    RUN(test_model_no_access_after_soft_reset);
    RUN(test_model_calibration_is_not_ready_during_the_nvm_copy);
    RUN(test_both_channels_match_the_manual);
    RUN(test_skip_pressure_reads_zero);
    RUN(test_skip_temperature_measures_once_in_init);
    RUN(test_skip_temperature_reuses_the_one_from_init);
    RUN(test_read_gets_this_measurement);
    RUN(test_read_gets_this_measurement_with_no_delay);
    RUN(test_read_at_x16_finishes);
    RUN(test_read_at_x16_finishes_with_no_delay);
    RUN(test_init_at_power_on_reads_the_calibration);
    RUN(test_init_with_no_delay_after_the_caller_waited);
    RUN(test_read_right_after_reset);
    RUN(test_read_right_after_reset_with_no_delay);
    RUN(test_init_right_after_reset_reads_the_calibration);
    RUN(test_skip_both_refused);
TEST_MAIN_END()
