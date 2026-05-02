// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "bme680.h"

/* ---------- chip-specific mock setup ---------- */

/*
 * Calibration register layout (from BME680 datasheet):
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

static const bme680_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

static bme680_t dev;

static void setup(void)
{
    mock_reset();
    bme680_init(&dev, &mock_i2c, BME680_ADDR_LOW);
}

/*
 * Plant ADC data into mock registers matching code's 13-byte read from 0x1F.
 * Data layout:
 *   0x1D = meas_status (new_data = bit 7, set separately)
 *   0x1F..0x21 = pressure (20-bit)
 *   0x22..0x24 = temperature (20-bit)
 *   0x25..0x26 = humidity (16-bit)
 *   0x2A = gas_r MSB
 *   0x2B = gas_r LSB[7:6] | gas_valid[5] | gas_range[3:0]
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
    mock_regs[0x2A] = (uint8_t)(adc_gas >> 2);
    mock_regs[0x2B] = (uint8_t)(((adc_gas & 0x03) << 6) |
                                (gas_valid ? 0x20 : 0x00) |
                                (gas_range & 0x0F));
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(e, BME680_OK);
}

TEST(test_init_chip_id)
{
    mock_reset();
    bme680_t d;
    bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(mock_regs[0xD0], 0x61);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0xD0] = 0x00;
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(e, BME680_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(e, BME680_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(bme680_init(NULL, &mock_i2c, 0x76), BME680_ERR_PARAM);
    bme680_t d;
    ASSERT_EQ(bme680_init(&d, NULL, 0x76), BME680_ERR_PARAM);
    bme680_data_t data;
    ASSERT_EQ(bme680_read(NULL, &data), BME680_ERR_PARAM);
    ASSERT_EQ(bme680_read(&d, NULL), BME680_ERR_PARAM);
    ASSERT_EQ(bme680_set_oversampling(NULL, 1, 1, 1), BME680_ERR_PARAM);
    ASSERT_EQ(bme680_set_gas_heater(NULL, 300, 100), BME680_ERR_PARAM);
    ASSERT_EQ(bme680_reset(NULL), BME680_ERR_PARAM);
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
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(e, BME680_ERR_I2C);
}

TEST(test_read_temperature)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);

    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_TRUE(data.temperature_c > 0.0f && data.temperature_c < 60.0f);
}

TEST(test_read_humidity)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);

    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_TRUE(data.humidity_pct >= 0.0f && data.humidity_pct <= 100.0f);
}

TEST(test_read_pressure)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);

    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_TRUE(data.pressure_pa > 30000.0f && data.pressure_pa < 130000.0f);
}

TEST(test_read_gas_valid)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);

    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_TRUE(data.gas_resistance > 0.0f);
}

TEST(test_read_gas_invalid)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, false);

    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_TRUE(data.gas_resistance == 0.0f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_ERR_I2C);
}

TEST(test_oversampling_config)
{
    setup();
    bme680_err_t e = bme680_set_oversampling(&dev, 5, 3, 4);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_EQ(mock_regs[0x72] & 0x07, 3);
    ASSERT_EQ((mock_regs[0x74] >> 5) & 0x07, 5);
    ASSERT_EQ((mock_regs[0x74] >> 2) & 0x07, 4);
}

TEST(test_oversampling_invalid)
{
    setup();
    ASSERT_EQ(bme680_set_oversampling(&dev, 6, 1, 1), BME680_ERR_PARAM);
    ASSERT_EQ(bme680_set_oversampling(&dev, 1, 6, 1), BME680_ERR_PARAM);
    ASSERT_EQ(bme680_set_oversampling(&dev, 1, 1, 6), BME680_ERR_PARAM);
}

TEST(test_gas_heater_config)
{
    setup();
    bme680_err_t e = bme680_set_gas_heater(&dev, 300, 150);
    ASSERT_EQ(e, BME680_OK);
    /* gas_wait0: 150ms -> 150>63 -> dur=150>>2=37, factor=1 -> 37+(1<<6)=101=0x65 */
    ASSERT_EQ(mock_regs[0x64], 0x65);
    /* ctrl_gas1 should have run_gas bit set */
    ASSERT_EQ(mock_regs[0x71] & 0x10, 0x10);
}

TEST(test_gas_heater_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bme680_err_t e = bme680_set_gas_heater(&dev, 300, 150);
    ASSERT_EQ(e, BME680_ERR_I2C);
}

TEST(test_reset)
{
    setup();
    bme680_err_t e = bme680_reset(&dev);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_EQ(mock_regs[0xE0], 0xB6);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bme680_err_t e = bme680_reset(&dev);
    ASSERT_EQ(e, BME680_ERR_I2C);
}

TEST(test_addr_high)
{
    mock_reset();
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &mock_i2c, BME680_ADDR_HIGH);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_EQ(d.addr, BME680_ADDR_HIGH);
}

TEST_MAIN("bme680")
    RUN(test_init_ok);
    RUN(test_init_chip_id);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_calibration_loading);
    RUN(test_calibration_i2c_fail);
    RUN(test_read_temperature);
    RUN(test_read_humidity);
    RUN(test_read_pressure);
    RUN(test_read_gas_valid);
    RUN(test_read_gas_invalid);
    RUN(test_read_i2c_fail);
    RUN(test_oversampling_config);
    RUN(test_oversampling_invalid);
    RUN(test_gas_heater_config);
    RUN(test_gas_heater_i2c_fail);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_high);
TEST_MAIN_END()
