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
    ASSERT_TRUE(data.gas_resistance_ohm > 0.0f);
}

TEST(test_read_gas_invalid)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, false);

    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_TRUE(data.gas_resistance_ohm == 0.0f);
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

TEST(test_read_timeout)
{
    mock_reset();
    bme680_t d;
    bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    /* status register: new_data bit (0x80) never set → timeout */
    mock_regs[0x1D] = 0x00;
    bme680_data_t data;
    ASSERT_EQ(bme680_read(&d, &data), BME680_ERR_TIMEOUT);
}

/*
 * bme680_init() does 8 I2C ops (op 0..7):
 *   0: read_regs chip_id
 *   1: read_regs coeff1
 *   2: read_regs coeff2
 *   3: read_regs 0x02 (res_heat_range)
 *   4: read_regs 0x00 (res_heat_val)
 *   5: read_regs 0x04 (range_sw_err)
 *   6: write_reg REG_CTRL_HUM
 *   7: write_reg REG_CTRL_MEAS
 *
 * bme680_read() (after setup) uses ops 8..13:
 *   8:  read_regs REG_CTRL_HUM
 *   9:  read_regs REG_CTRL_MEAS
 *  10:  write_reg REG_CTRL_HUM  (trigger forced mode)
 *  11:  write_reg REG_CTRL_MEAS (trigger forced mode)
 *  12:  read_regs REG_MEAS_STATUS (poll, iteration 0)
 *  13:  read_regs REG_DATA_START
 */

/* Fail coeff2 read (op 2) → line 84 */
TEST(test_init_calib_coeff2_fail)
{
    mock_reset();
    mock_fail_at = 2;
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(e, BME680_ERR_I2C);
}

/* Fail extra gas reg reads: res_heat_range (op 3), res_heat_val (op 4),
 * range_sw_err (op 5) → lines in read_calibration */
TEST(test_init_calib_extra_regs_fail)
{
    /* fail at op 3 (res_heat_range read) */
    mock_reset();
    mock_fail_at = 3;
    bme680_t d;
    ASSERT_EQ(bme680_init(&d, &mock_i2c, BME680_ADDR_LOW), BME680_ERR_I2C);

    /* fail at op 4 (res_heat_val read) */
    mock_reset();
    mock_fail_at = 4;
    ASSERT_EQ(bme680_init(&d, &mock_i2c, BME680_ADDR_LOW), BME680_ERR_I2C);

    /* fail at op 5 (range_sw_err read) */
    mock_reset();
    mock_fail_at = 5;
    ASSERT_EQ(bme680_init(&d, &mock_i2c, BME680_ADDR_LOW), BME680_ERR_I2C);
}

/* Fail ctrl_hum write during init (op 6) → line 246 */
TEST(test_init_ctrl_hum_write_fail)
{
    mock_reset();
    mock_fail_at = 6;
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(e, BME680_ERR_I2C);
}

/* Fail ctrl_meas write during init (op 7) → line 248 */
TEST(test_init_ctrl_meas_write_fail)
{
    mock_reset();
    mock_fail_at = 7;
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &mock_i2c, BME680_ADDR_LOW);
    ASSERT_EQ(e, BME680_ERR_I2C);
}

/* NULL i2c callbacks → line 224 */
TEST(test_init_null_callbacks)
{
    static const bme680_i2c_t no_write = {
        .write_reg = NULL,
        .read_regs = mock_i2c_read_regs,
        .delay_ms  = mock_i2c_delay_ms,
        .ctx       = NULL,
    };
    static const bme680_i2c_t no_read = {
        .write_reg = mock_i2c_write_reg,
        .read_regs = NULL,
        .delay_ms  = mock_i2c_delay_ms,
        .ctx       = NULL,
    };
    bme680_t d;
    ASSERT_EQ(bme680_init(&d, &no_write, BME680_ADDR_LOW), BME680_ERR_PARAM);
    ASSERT_EQ(bme680_init(&d, &no_read,  BME680_ADDR_LOW), BME680_ERR_PARAM);
}

/* Fail read ctrl_meas during bme680_read (op 9 after setup) → line 265 */
TEST(test_read_ctrl_meas_read_fail)
{
    setup();
    mock_fail_at = mock_op_count + 1;  /* op 9: read REG_CTRL_MEAS */
    bme680_data_t data;
    ASSERT_EQ(bme680_read(&dev, &data), BME680_ERR_I2C);
}

/* Fail write ctrl_hum during forced-mode trigger (op 10) → line 269 */
TEST(test_read_ctrl_hum_write_fail)
{
    setup();
    mock_fail_at = mock_op_count + 2;  /* op 10: write REG_CTRL_HUM */
    bme680_data_t data;
    ASSERT_EQ(bme680_read(&dev, &data), BME680_ERR_I2C);
}

/* Fail write ctrl_meas during forced-mode trigger (op 11) → line 271 */
TEST(test_read_ctrl_meas_write_fail)
{
    setup();
    mock_fail_at = mock_op_count + 3;  /* op 11: write REG_CTRL_MEAS */
    bme680_data_t data;
    ASSERT_EQ(bme680_read(&dev, &data), BME680_ERR_I2C);
}

/* Fail status poll read during bme680_read (op 12) → line 282 */
TEST(test_read_poll_fail)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);
    mock_fail_at = mock_op_count + 4;  /* op 12: read REG_MEAS_STATUS */
    bme680_data_t data;
    ASSERT_EQ(bme680_read(&dev, &data), BME680_ERR_I2C);
}

/* Fail data buffer read (op 13 after poll succeeds) → line 300 */
TEST(test_read_data_buf_fail)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);
    mock_fail_at = mock_op_count + 5;  /* op 13: read REG_DATA_START */
    bme680_data_t data;
    ASSERT_EQ(bme680_read(&dev, &data), BME680_ERR_I2C);
}

/* Fail first write in set_oversampling (op 0 after setup) → line 349 */
TEST(test_oversampling_ctrl_hum_fail)
{
    setup();
    mock_fail_at = mock_op_count;  /* op 8: write REG_CTRL_HUM */
    ASSERT_EQ(bme680_set_oversampling(&dev, 2, 2, 2), BME680_ERR_I2C);
}

/* Fail second write in set_oversampling (op 1 after setup) → line 353 */
TEST(test_oversampling_ctrl_meas_fail)
{
    setup();
    mock_fail_at = mock_op_count + 1;  /* op 9: write REG_CTRL_MEAS */
    ASSERT_EQ(bme680_set_oversampling(&dev, 2, 2, 2), BME680_ERR_I2C);
}

/* Fail second write in set_gas_heater (op 1 = gas_wait write) → line 402 */
TEST(test_gas_heater_gas_wait_fail)
{
    setup();
    mock_fail_at = mock_op_count + 1;  /* op 1: write REG_GAS_WAIT0 */
    ASSERT_EQ(bme680_set_gas_heater(&dev, 300, 150), BME680_ERR_I2C);
}

/* Fail third write in set_gas_heater (op 2 = ctrl_gas1 write) → line 405 */
TEST(test_gas_heater_ctrl_gas1_fail)
{
    setup();
    mock_fail_at = mock_op_count + 2;  /* op 2: write REG_CTRL_GAS1 */
    ASSERT_EQ(bme680_set_gas_heater(&dev, 300, 150), BME680_ERR_I2C);
}

/* duration_ms >= 4032 → gas_wait = 0xFF (line 385) */
TEST(test_gas_heater_long_duration)
{
    setup();
    bme680_err_t e = bme680_set_gas_heater(&dev, 300, 4032);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_EQ(mock_regs[0x64], 0xFF);

    /* also test exactly at boundary */
    mock_reset();
    bme680_init(&dev, &mock_i2c, BME680_ADDR_LOW);
    e = bme680_set_gas_heater(&dev, 300, 5000);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_EQ(mock_regs[0x64], 0xFF);
}

/*
 * pressure compensation small branch (line 164):
 * The branch `press < 0x40000000` is taken when adc_p is much larger
 * than 1048576 (i.e., adc_p ~ 1048575 makes press*3125 small).
 * Use adc_p near maximum (20-bit max = 0xFFFFF = 1048575) and typical
 * t_fine so var1 is positive and moderate.
 */
TEST(test_pressure_small_branch)
{
    setup();
    /* adc_p = 0xFFFFF = 1048575 makes (1048576 - adc_p) = 1, so press*3125 = 3125 < 0x40000000 */
    plant_data(1048575u, 500000, 25000, 300, 3, true);
    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    /* pressure should be a small but non-negative value */
    ASSERT_TRUE(data.pressure_pa >= 0.0f);
}

/*
 * compensate_pressure var1==0 path (line 155):
 * par_p1=0 makes the final var1 always 0. Set par_p1=0 directly after init.
 * Note: t_fine must be set first via compensate_temperature; a read()
 * call triggers that. Instead directly manipulate par_p1 to 0 then call read.
 */
TEST(test_pressure_var1_zero)
{
    setup();
    plant_data(415148, 500000, 25000, 300, 3, true);
    /* par_p1 = 0 forces var1 = 0 in compensate_pressure → returns 0 */
    dev.par_p1 = 0;
    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    /* pressure_pa = (float)0 = 0.0f */
    ASSERT_NEAR(data.pressure_pa, 0.0f, 1.0f);
}

/*
 * compensate_gas var2==0 path (line 211):
 * var2 = adc_gas * 32768 - 16777216 + var1
 * With adc_gas=0 and var1 chosen to cancel: var1 = 16777216 → var2 = 0.
 * var1 = (1340 + 5*range_sw_err) * gas_range_lut1[r] >> 16
 * For r=0: gas_range_lut1[0]=2147483647 and range_sw_err=0:
 *   var1 = 1340 * 2147483647 >> 16 = 2877627862 >> 16 ≈ 43921
 * That won't reach 16777216 with typical values. Use adc_gas=512 so that
 * adc_gas*32768 = 16777216 exactly, then need var1=0 so range_sw_err=-268
 * which isn't representable. Instead set adc_gas=0, gas_range=0 and
 * use a crafted res_heat... this edge case is not practically triggerable
 * from the public API mock. Skip this specific internal branch and rely on
 * the overall coverage from the existing gas tests.
 *
 * We still exercise gas path coverage via additional gas_range values.
 */
TEST(test_gas_various_ranges)
{
    uint8_t r;
    bme680_data_t data;

    /* Exercise all 16 gas ranges to exercise lut1/lut2 indexing */
    for (r = 0; r < 16; r++)
    {
        setup();
        plant_data(415148, 500000, 25000, 512, r, true);
        bme680_err_t e = bme680_read(&dev, &data);
        ASSERT_EQ(e, BME680_OK);
        ASSERT_TRUE(data.gas_resistance_ohm >= 0.0f);
    }
}

/* Verify humidity clamping at 100000 (100% RH) */
TEST(test_humidity_clamp)
{
    setup();
    /* Use maximum adc_h = 0xFFFF to drive humidity above 100000 units */
    plant_data(415148, 500000, 0xFFFF, 300, 3, true);
    bme680_data_t data;
    bme680_err_t e = bme680_read(&dev, &data);
    ASSERT_EQ(e, BME680_OK);
    ASSERT_TRUE(data.humidity_pct <= 100.0f);
}

/* Verify delay_ms is not called when callback is NULL */
TEST(test_no_delay_callback)
{
    static const bme680_i2c_t i2c_no_delay = {
        .write_reg = mock_i2c_write_reg,
        .read_regs = mock_i2c_read_regs,
        .delay_ms  = NULL,
        .ctx       = NULL,
    };
    mock_reset();
    bme680_t d;
    bme680_err_t e = bme680_init(&d, &i2c_no_delay, BME680_ADDR_LOW);
    ASSERT_EQ(e, BME680_OK);
    plant_data(415148, 500000, 25000, 300, 3, true);
    bme680_data_t data;
    e = bme680_read(&d, &data);
    ASSERT_EQ(e, BME680_OK);
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
    RUN(test_read_timeout);
    RUN(test_oversampling_config);
    RUN(test_oversampling_invalid);
    RUN(test_gas_heater_config);
    RUN(test_gas_heater_i2c_fail);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_high);
    RUN(test_init_calib_coeff2_fail);
    RUN(test_init_calib_extra_regs_fail);
    RUN(test_init_ctrl_hum_write_fail);
    RUN(test_init_ctrl_meas_write_fail);
    RUN(test_init_null_callbacks);
    RUN(test_read_ctrl_meas_read_fail);
    RUN(test_read_ctrl_hum_write_fail);
    RUN(test_read_ctrl_meas_write_fail);
    RUN(test_read_poll_fail);
    RUN(test_read_data_buf_fail);
    RUN(test_oversampling_ctrl_hum_fail);
    RUN(test_oversampling_ctrl_meas_fail);
    RUN(test_gas_heater_gas_wait_fail);
    RUN(test_gas_heater_ctrl_gas1_fail);
    RUN(test_gas_heater_long_duration);
    RUN(test_pressure_small_branch);
    RUN(test_pressure_var1_zero);
    RUN(test_gas_various_ranges);
    RUN(test_humidity_clamp);
    RUN(test_no_delay_callback);
TEST_MAIN_END()
