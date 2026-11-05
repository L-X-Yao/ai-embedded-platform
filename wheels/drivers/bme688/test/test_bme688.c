// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "bme688.h"

/* ---------- chip-specific mock setup ---------- */

/*
 * Calibration register layout (from BME688 datasheet):
 *   Coeff1: 0x89..0xA1 (25 bytes, indices c1[0]..c1[24])
 *   Coeff2: 0xE1..0xF0 (16 bytes, indices c2[0]..c2[15])
 *   Extra:  0x00 (res_heat_val), 0x02 (res_heat_range), 0x04 (range_sw_err)
 *
 * Realistic calibration values chosen to produce plausible output:
 *   par_t1=26041, par_t2=26285, par_t3=-3
 *   par_p1=36197, par_p2=-10474, par_p3=88
 *   par_p4=-3636, par_p5=143, par_p6=30, par_p7=28
 *   par_p8=-3472, par_p9=-1750, par_p10=30
 *   par_h1=823, par_h2=1016, par_h3=0, par_h4=45, par_h5=20, par_h6=120, par_h7=-100
 *   par_gh1=0, par_gh2=-3100, par_gh3=0
 */

static void plant_coeff1(void)
{
    /* par_t2 = 26285 = 0x66AD */
    mock_regs[0x8A] = 0xAD;
    mock_regs[0x8B] = 0x66;
    /* par_t3 = -3 */
    mock_regs[0x8C] = 0xFD;
    /* par_p1 = 36197 = 0x8D65 */
    mock_regs[0x8E] = 0x65;
    mock_regs[0x8F] = 0x8D;
    /* par_p2 = -10474 = 0xD716 */
    mock_regs[0x90] = 0x16;
    mock_regs[0x91] = 0xD7;
    /* par_p3 = 88 */
    mock_regs[0x92] = 0x58;
    /* par_p4 = -3636 = 0xF1CC */
    mock_regs[0x94] = 0xCC;
    mock_regs[0x95] = 0xF1;
    /* par_p5 = 143 = 0x008F */
    mock_regs[0x96] = 0x8F;
    mock_regs[0x97] = 0x00;
    /* par_p7 = 28 */
    mock_regs[0x98] = 0x1C;
    /* par_p6 = 30 */
    mock_regs[0x99] = 0x1E;
    /* par_p8 = -3472 = 0xF270 */
    mock_regs[0x9C] = 0x70;
    mock_regs[0x9D] = 0xF2;
    /* par_p9 = -1750 = 0xF92A */
    mock_regs[0x9E] = 0x2A;
    mock_regs[0x9F] = 0xF9;
    /* par_p10 = 30 */
    mock_regs[0xA0] = 0x1E;
}

static void plant_coeff2(void)
{
    mock_regs[0xE1] = 0x3F;  /* c2[0]: par_h2 high byte */
    mock_regs[0xE2] = 0x87;  /* c2[1]: par_h2_low<<4 | par_h1_low */
    mock_regs[0xE3] = 0x33;  /* c2[2]: par_h1 high byte */
    mock_regs[0xE4] = 0x00;  /* par_h3 = 0 */
    mock_regs[0xE5] = 0x2D;  /* par_h4 = 45 */
    mock_regs[0xE6] = 0x14;  /* par_h5 = 20 */
    mock_regs[0xE7] = 0x78;  /* par_h6 = 120 */
    mock_regs[0xE8] = 0x9C;  /* par_h7 = -100 */
    mock_regs[0xE9] = 0xB9;  /* par_t1 low = 0xB9 */
    mock_regs[0xEA] = 0x65;  /* par_t1 high = 0x65 -> 26041 */
    mock_regs[0xEB] = 0xE4;  /* par_gh2 low */
    mock_regs[0xEC] = 0xF3;  /* par_gh2 high -> -3100 */
    mock_regs[0xED] = 0x00;  /* par_gh1 = 0 */
    mock_regs[0xEE] = 0x00;  /* par_gh3 = 0 */
}

static void plant_extra_gas_regs(void)
{
    mock_regs[0x02] = 0x10;  /* res_heat_range = 1 (bits[5:4]) */
    mock_regs[0x00] = 0x32;  /* res_heat_val = 50 */
    mock_regs[0x04] = 0x00;  /* range_sw_err = 0 */
}

static const bme688_cfg_t default_cfg = {
    .osrs_t = 1,
    .osrs_p = 1,
    .osrs_h = 1,
    .filter = 0,
    .gas_heater_temp_c = 0,
    .gas_heater_dur_ms = 0,
    .addr = BME688_ADDR_LOW,
};

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_delay_ms_count = 0;

    mock_regs[0xD0] = 0x61;  /* chip ID */

    plant_coeff1();
    plant_coeff2();
    plant_extra_gas_regs();

    mock_regs[0x72] = 0x00;
    mock_regs[0x74] = 0x00;
}

static const bme688_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static bme688_t dev;

static void setup(void)
{
    mock_reset();
    bme688_init(&dev, &mock_i2c, &default_cfg);
}

/*
 * Plant ADC data into mock registers matching code's 15-byte read from 0x1F.
 * Data layout:
 *   0x1D = meas_status (new_data = bit 7, set separately)
 *   0x1F..0x21 = pressure (20-bit)
 *   0x22..0x24 = temperature (20-bit)
 *   0x25..0x26 = humidity (16-bit)
 *   0x2C = gas_r MSB
 *   0x2D = gas_r LSB[7:6] | gas_valid[5] | gas_range[3:0]
 */
static void plant_data(uint32_t adc_p, uint32_t adc_t,
                       uint16_t adc_h, uint16_t adc_gas,
                       uint8_t gas_range, bool gas_valid)
{
    /* meas_status: set new_data bit so polling succeeds */
    mock_regs[0x1D] = 0x80;
    /* pressure: 20-bit */
    mock_regs[0x1F] = (uint8_t)(adc_p >> 12);
    mock_regs[0x20] = (uint8_t)(adc_p >> 4);
    mock_regs[0x21] = (uint8_t)((adc_p & 0x0F) << 4);
    /* temperature: 20-bit */
    mock_regs[0x22] = (uint8_t)(adc_t >> 12);
    mock_regs[0x23] = (uint8_t)(adc_t >> 4);
    mock_regs[0x24] = (uint8_t)((adc_t & 0x0F) << 4);
    /* humidity: 16-bit */
    mock_regs[0x25] = (uint8_t)(adc_h >> 8);
    mock_regs[0x26] = (uint8_t)(adc_h & 0xFF);
    /* gas: 10-bit ADC + valid + range */
    mock_regs[0x2C] = (uint8_t)(adc_gas >> 2);
    mock_regs[0x2D] = (uint8_t)(((adc_gas & 0x03) << 6) |
                                (gas_valid ? 0x20 : 0x00) |
                                (gas_range & 0x0F));
}

/* ---------- tests ---------- */

TEST(test_init_null)
{
    ASSERT_EQ(bme688_init(NULL, &mock_i2c, &default_cfg), BME688_ERR_PARAM);
    bme688_t d;
    ASSERT_EQ(bme688_init(&d, NULL, &default_cfg), BME688_ERR_PARAM);
    ASSERT_EQ(bme688_init(&d, &mock_i2c, NULL), BME688_ERR_PARAM);
    bme688_data_t data;
    ASSERT_EQ(bme688_read(NULL, &data), BME688_ERR_PARAM);
    ASSERT_EQ(bme688_read(&d, NULL), BME688_ERR_PARAM);
    ASSERT_EQ(bme688_set_gas_heater(NULL, 300, 100), BME688_ERR_PARAM);
    ASSERT_EQ(bme688_reset(NULL), BME688_ERR_PARAM);
}

TEST(test_init_null_callbacks)
{
    static const bme688_i2c_t no_write = {
        .write_reg = NULL,
        .read_regs = mock_i2c_read_regs,
        .delay_ms  = mock_i2c_delay_ms,
        .ctx       = NULL,
    };
    static const bme688_i2c_t no_read = {
        .write_reg = mock_i2c_write_reg,
        .read_regs = NULL,
        .delay_ms  = mock_i2c_delay_ms,
        .ctx       = NULL,
    };
    bme688_t d;
    ASSERT_EQ(bme688_init(&d, &no_write, &default_cfg), BME688_ERR_PARAM);
    ASSERT_EQ(bme688_init(&d, &no_read,  &default_cfg), BME688_ERR_PARAM);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0xD0] = 0x00;
    bme688_t d;
    bme688_err_t e = bme688_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME688_ERR_ID);
}

TEST(test_init_ok)
{
    mock_reset();
    bme688_t d;
    bme688_err_t e = bme688_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME688_OK);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    bme688_t d;
    bme688_err_t e = bme688_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME688_ERR_HAL);
}

TEST(test_calibration_loading)
{
    setup();
    ASSERT_EQ(dev.par_t1, 26041);
    ASSERT_EQ(dev.par_t2, 26285);
    ASSERT_EQ(dev.par_t3, -3);
    ASSERT_EQ(dev.par_p1, 36197);
    ASSERT_EQ(dev.par_p2, -10474);
    ASSERT_EQ(dev.par_p3, 88);
    ASSERT_EQ(dev.par_h6, 120);
    ASSERT_EQ(dev.par_h7, -100);
    ASSERT_EQ(dev.par_gh2, -3100);
}

TEST(test_calibration_i2c_fail)
{
    mock_reset();
    mock_fail_at = 1;
    bme688_t d;
    bme688_err_t e = bme688_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME688_ERR_HAL);
}

TEST(test_read_tph)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);

    bme688_data_t data;
    bme688_err_t e = bme688_read(&dev, &data);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_TRUE(data.temperature_c > 0.0f && data.temperature_c < 60.0f);
    ASSERT_TRUE(data.pressure_pa > 30000.0f && data.pressure_pa < 130000.0f);
    ASSERT_TRUE(data.humidity_pct >= 0.0f && data.humidity_pct <= 100.0f);
}

TEST(test_gas_heater)
{
    setup();
    bme688_err_t e = bme688_set_gas_heater(&dev, 300, 150);
    ASSERT_EQ(e, BME688_OK);
    /* gas_wait0: 150ms -> 150>63 -> dur=150>>2=37, factor=1 -> 37+(1<<6)=101=0x65 */
    ASSERT_EQ(mock_regs[0x64], 0x65);
    /* manual p40: run_gas is bit 5; p19: nb_conv 0 selects heater step 0 */
    ASSERT_EQ_HEX(mock_regs[0x71], 0x20);

    /* now read with gas data planted */
    plant_data(415148, 500000, 25000, 300, 3, true);
    bme688_data_t data;
    e = bme688_read(&dev, &data);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_TRUE(data.gas_valid);
    ASSERT_TRUE(data.gas_resistance_ohm > 0);
}

TEST(test_oversampling_config)
{
    mock_reset();
    bme688_cfg_t cfg = {
        .osrs_t = 5, .osrs_p = 4, .osrs_h = 3,
        .filter = 2, .gas_heater_temp_c = 0, .gas_heater_dur_ms = 0,
        .addr = BME688_ADDR_LOW,
    };
    bme688_t d;
    bme688_err_t e = bme688_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BME688_OK);
    /* verify ctrl_hum */
    ASSERT_EQ(mock_regs[0x72] & 0x07, 3);
    /* verify ctrl_meas: osrs_t[7:5] = 5, osrs_p[4:2] = 4 */
    ASSERT_EQ((mock_regs[0x74] >> 5) & 0x07, 5);
    ASSERT_EQ((mock_regs[0x74] >> 2) & 0x07, 4);
    /* verify IIR filter config: filter[4:2] = 2 */
    ASSERT_EQ((mock_regs[0x75] >> 2) & 0x07, 2);
}

TEST(test_oversampling_invalid)
{
    mock_reset();
    bme688_t d;
    bme688_cfg_t cfg = default_cfg;
    cfg.osrs_t = 6;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &cfg), BME688_ERR_PARAM);
    cfg = default_cfg;
    cfg.osrs_p = 6;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &cfg), BME688_ERR_PARAM);
    cfg = default_cfg;
    cfg.osrs_h = 6;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &cfg), BME688_ERR_PARAM);
    cfg = default_cfg;
    cfg.filter = 8;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &cfg), BME688_ERR_PARAM);
}

TEST(test_read_gas_invalid)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, false);

    bme688_data_t data;
    bme688_err_t e = bme688_read(&dev, &data);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_FALSE(data.gas_valid);
    ASSERT_EQ((long long)data.gas_resistance_ohm, 0);
}

/* Manual p42, p44: field 0 holds gas_r and its status at 0x2C and 0x2D */
TEST(test_gas_status_from_field0)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, false);
    mock_regs[0x2A] = 0xFF;  /* the BME680 locations, nothing on a BME688 */
    mock_regs[0x2B] = 0xFF;

    bme688_data_t data;
    ASSERT_EQ(bme688_read(&dev, &data), BME688_OK);
    ASSERT_FALSE(data.gas_valid);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bme688_data_t data;
    bme688_err_t e = bme688_read(&dev, &data);
    ASSERT_EQ(e, BME688_ERR_HAL);
}

TEST(test_read_timeout)
{
    mock_reset();
    bme688_t d;
    bme688_init(&d, &mock_i2c, &default_cfg);
    /* status register: new_data bit (0x80) never set */
    mock_regs[0x1D] = 0x00;
    bme688_data_t data;
    ASSERT_EQ(bme688_read(&d, &data), BME688_ERR_TIMEOUT);
}

TEST(test_reset)
{
    setup();
    bme688_err_t e = bme688_reset(&dev);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_EQ(mock_regs[0xE0], 0xB6);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bme688_err_t e = bme688_reset(&dev);
    ASSERT_EQ(e, BME688_ERR_HAL);
}

TEST(test_addr_high)
{
    mock_reset();
    bme688_t d;
    bme688_cfg_t cfg = default_cfg;
    cfg.addr = BME688_ADDR_HIGH;
    bme688_err_t e = bme688_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_EQ(d.cfg.addr, BME688_ADDR_HIGH);
}

TEST(test_gas_heater_long_duration)
{
    setup();
    bme688_err_t e = bme688_set_gas_heater(&dev, 300, 4032);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_EQ(mock_regs[0x64], 0xFF);

    mock_reset();
    bme688_init(&dev, &mock_i2c, &default_cfg);
    e = bme688_set_gas_heater(&dev, 300, 5000);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_EQ(mock_regs[0x64], 0xFF);
}

TEST(test_gas_heater_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bme688_err_t e = bme688_set_gas_heater(&dev, 300, 150);
    ASSERT_EQ(e, BME688_ERR_HAL);
}

TEST(test_init_with_gas_heater)
{
    mock_reset();
    bme688_cfg_t cfg = {
        .osrs_t = 1, .osrs_p = 1, .osrs_h = 1,
        .filter = 0,
        .gas_heater_temp_c = 300,
        .gas_heater_dur_ms = 100,
        .addr = BME688_ADDR_LOW,
    };
    bme688_t d;
    bme688_err_t e = bme688_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BME688_OK);
    /* manual p40: run_gas is bit 5; p19: nb_conv 0 selects heater step 0 */
    ASSERT_EQ_HEX(mock_regs[0x71], 0x20);
}

TEST(test_humidity_clamp)
{
    setup();
    /* Use maximum adc_h = 0xFFFF to drive humidity above 100000 units */
    plant_data(415148, 500000, 0xFFFF, 300, 3, true);
    bme688_data_t data;
    bme688_err_t e = bme688_read(&dev, &data);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_TRUE(data.humidity_pct <= 100.0f);
}

TEST(test_no_delay_callback)
{
    static const bme688_i2c_t i2c_no_delay = {
        .write_reg = mock_i2c_write_reg,
        .read_regs = mock_i2c_read_regs,
        .delay_ms  = NULL,
        .ctx       = NULL,
    };
    mock_reset();
    bme688_t d;
    bme688_err_t e = bme688_init(&d, &i2c_no_delay, &default_cfg);
    ASSERT_EQ(e, BME688_OK);
    plant_data(415148, 500000, 25000, 300, 3, true);
    bme688_data_t data;
    e = bme688_read(&d, &data);
    ASSERT_EQ(e, BME688_OK);
}

TEST(test_gas_various_ranges)
{
    uint8_t r;
    bme688_data_t data;

    /* Exercise all 16 gas ranges */
    for (r = 0; r < 16; r++)
    {
        setup();
        plant_data(415148, 500000, 25000, 512, r, true);
        bme688_err_t e = bme688_read(&dev, &data);
        ASSERT_EQ(e, BME688_OK);
        ASSERT_TRUE(data.gas_resistance_ohm > 0);
    }
}

/* Expected values from the integer code on manual p28 and p29:
 * (10000 * (262144 >> range)) / (4096 + 3 * (adc - 512)) * 100 */
TEST(test_gas_resistance_formula)
{
    static const struct {
        uint16_t adc;
        uint8_t  range;
        uint32_t ohm;
    } v[] = {
        {  500,  4,   4035400u },  /* 163840000 / 4060 = 40354 */
        {  300,  3,   9470500u },  /* 327680000 / 3460 = 94705 */
        {  512,  0,  64000000u },  /* 2621440000 / 4096 = 640000 */
        {    0,  0, 102400000u },  /* lowest ADC code, largest result */
        { 1023, 15,      1400u },  /* 80000 / 5629 = 14 */
    };

    for (unsigned i = 0; i < sizeof(v) / sizeof(v[0]); i++)
    {
        setup();
        plant_data(415148, 500000, 25000, v[i].adc, v[i].range, true);
        bme688_data_t data;
        ASSERT_EQ(bme688_read(&dev, &data), BME688_OK);
        ASSERT_EQ(data.gas_resistance_ohm, v[i].ohm);
    }
}

TEST(test_pressure_var1_zero)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);
    /* par_p1 = 0 forces var1 = 0 in compensate_pressure -> returns 0 */
    dev.par_p1 = 0;
    bme688_data_t data;
    bme688_err_t e = bme688_read(&dev, &data);
    ASSERT_EQ(e, BME688_OK);
    ASSERT_NEAR(data.pressure_pa, 0.0f, 1.0f);
}

TEST(test_zero_init_safety)
{
    bme688_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    bme688_t zero_dev;
    memset(&zero_dev, 0, sizeof(zero_dev));
    ASSERT_TRUE(bme688_init(&zero_dev, &zero_hal, &default_cfg) < 0);
}

TEST(test_error_codes)
{
    ASSERT_EQ(-1, (int)BME688_ERR_HAL);
    ASSERT_EQ(-2, (int)BME688_ERR_PARAM);
    ASSERT_EQ(-3, (int)BME688_ERR_ID);
    ASSERT_EQ(-4, (int)BME688_ERR_TIMEOUT);
}

/* Fail coeff2 read (op 2) */
TEST(test_init_calib_coeff2_fail)
{
    mock_reset();
    mock_fail_at = 2;
    bme688_t d;
    bme688_err_t e = bme688_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME688_ERR_HAL);
}

/* Fail extra gas reg reads (ops 3, 4, 5) */
TEST(test_init_calib_extra_regs_fail)
{
    mock_reset();
    mock_fail_at = 3;
    bme688_t d;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &default_cfg), BME688_ERR_HAL);

    mock_reset();
    mock_fail_at = 4;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &default_cfg), BME688_ERR_HAL);

    mock_reset();
    mock_fail_at = 5;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &default_cfg), BME688_ERR_HAL);
}

/* Fail config write (op 6) */
TEST(test_init_config_write_fail)
{
    mock_reset();
    mock_fail_at = 6;
    bme688_t d;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &default_cfg), BME688_ERR_HAL);
}

/* Fail ctrl_hum write during init (op 7) */
TEST(test_init_ctrl_hum_write_fail)
{
    mock_reset();
    mock_fail_at = 7;
    bme688_t d;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &default_cfg), BME688_ERR_HAL);
}

/* Fail ctrl_meas write during init (op 8) */
TEST(test_init_ctrl_meas_write_fail)
{
    mock_reset();
    mock_fail_at = 8;
    bme688_t d;
    ASSERT_EQ(bme688_init(&d, &mock_i2c, &default_cfg), BME688_ERR_HAL);
}

/* Fail gas wait write in set_gas_heater (op 1) */
TEST(test_gas_heater_gas_wait_fail)
{
    setup();
    mock_fail_at = mock_op_count + 1;
    ASSERT_EQ(bme688_set_gas_heater(&dev, 300, 150), BME688_ERR_HAL);
}

/* Fail ctrl_gas1 write in set_gas_heater (op 2) */
TEST(test_gas_heater_ctrl_gas1_fail)
{
    setup();
    mock_fail_at = mock_op_count + 2;
    ASSERT_EQ(bme688_set_gas_heater(&dev, 300, 150), BME688_ERR_HAL);
}

/* Fail data buffer read after poll succeeds */
TEST(test_read_data_buf_fail)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);
    mock_fail_at = mock_op_count + 5;
    bme688_data_t data;
    ASSERT_EQ(bme688_read(&dev, &data), BME688_ERR_HAL);
}

TEST_MAIN("bme688")
    RUN(test_init_null);
    RUN(test_init_null_callbacks);
    RUN(test_init_wrong_id);
    RUN(test_init_ok);
    RUN(test_init_i2c_fail);
    RUN(test_calibration_loading);
    RUN(test_calibration_i2c_fail);
    RUN(test_read_tph);
    RUN(test_gas_heater);
    RUN(test_oversampling_config);
    RUN(test_oversampling_invalid);
    RUN(test_read_gas_invalid);
    RUN(test_gas_status_from_field0);
    RUN(test_read_i2c_fail);
    RUN(test_read_timeout);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_high);
    RUN(test_gas_heater_long_duration);
    RUN(test_gas_heater_i2c_fail);
    RUN(test_init_with_gas_heater);
    RUN(test_humidity_clamp);
    RUN(test_no_delay_callback);
    RUN(test_gas_various_ranges);
    RUN(test_gas_resistance_formula);
    RUN(test_pressure_var1_zero);
    RUN(test_zero_init_safety);
    RUN(test_error_codes);
    RUN(test_init_calib_coeff2_fail);
    RUN(test_init_calib_extra_regs_fail);
    RUN(test_init_config_write_fail);
    RUN(test_init_ctrl_hum_write_fail);
    RUN(test_init_ctrl_meas_write_fail);
    RUN(test_gas_heater_gas_wait_fail);
    RUN(test_gas_heater_ctrl_gas1_fail);
    RUN(test_read_data_buf_fail);
TEST_MAIN_END()
