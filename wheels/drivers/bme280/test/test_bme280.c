// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include <math.h>
#include "bme280.h"

/* ---------- chip-specific mock setup ---------- */

static bool mock_stays_forced;       /* the measurement never ends */
static bool mock_i2c_read_fail;      /* every read gets no ACK */

static void mock_reset(void)
{
    mock_i2c_reset();
    mock_stays_forced = false;

    /* BME280 chip ID */
    mock_regs[0xD0] = 0x60;

    /* Temperature + pressure calibration (0x88..0xA1) — same example as bmp280 */
    mock_regs[0x88] = 0x70; mock_regs[0x89] = 0x6B; /* dig_t1 = 27504 */
    mock_regs[0x8A] = 0x43; mock_regs[0x8B] = 0x67; /* dig_t2 = 26435 */
    mock_regs[0x8C] = 0x18; mock_regs[0x8D] = 0xFC; /* dig_t3 = -1000 */
    mock_regs[0x8E] = 0x7D; mock_regs[0x8F] = 0x8E; /* dig_p1 = 36477 */
    mock_regs[0x90] = 0x43; mock_regs[0x91] = 0xD6; /* dig_p2 */
    mock_regs[0x92] = 0xD0; mock_regs[0x93] = 0x0B; /* dig_p3 */
    mock_regs[0x94] = 0x27; mock_regs[0x95] = 0x0B; /* dig_p4 */
    mock_regs[0x96] = 0x8C; mock_regs[0x97] = 0x00; /* dig_p5 */
    mock_regs[0x98] = 0xF9; mock_regs[0x99] = 0xFF; /* dig_p6 */
    mock_regs[0x9A] = 0x8C; mock_regs[0x9B] = 0x3C; /* dig_p7 */
    mock_regs[0x9C] = 0xF8; mock_regs[0x9D] = 0xC6; /* dig_p8 */
    mock_regs[0x9E] = 0x70; mock_regs[0x9F] = 0x17; /* dig_p9 */
    /* 0xA0 reserved, 0xA1 = dig_h1 */
    mock_regs[0xA1] = 0x4B; /* dig_h1 = 75 */

    /* Humidity calibration (0xE1..0xE7) */
    mock_regs[0xE1] = 0x6E; mock_regs[0xE2] = 0x01; /* dig_h2 = 366 */
    mock_regs[0xE3] = 0x00;                           /* dig_h3 = 0   */
    mock_regs[0xE4] = 0x13; mock_regs[0xE5] = 0x62; /* dig_h4/h5    */
    mock_regs[0xE6] = 0x00;                           /* dig_h5 high  */
    mock_regs[0xE7] = 0x1E;                           /* dig_h6 = 30  */
}

static uint8_t mock_last_ctrl_meas;

/* p.15: "When the measurement is finished, the sensor returns to sleep mode".
 * Here a measurement is over as soon as it starts. */
static bool chip_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    if (!mock_i2c_write_reg(ctx, dev_addr, reg, val))
        return false;
    if (reg == 0xF4) {
        mock_last_ctrl_meas = val;
        if (!mock_stays_forced)
            mock_regs[0xF4] &= 0xFC;
    }
    return true;
}

static const bme280_i2c_t mock_i2c = {
    .write_reg = chip_write_reg,
    .read_regs = mock_i2c_read_regs,
    .ctx       = NULL,
};

static bool chip_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                           uint8_t *buf, uint16_t len)
{
    if (mock_i2c_read_fail)
        return false;
    return mock_i2c_read_regs(ctx, dev_addr, reg, buf, len);
}

/* The same chip with a delay callback */
static const bme280_i2c_t mock_delay_i2c = {
    .write_reg = chip_write_reg,
    .read_regs = chip_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */
static bme280_t dev;
static bme280_cfg_t default_cfg = {
    .addr   = BME280_ADDR_LOW,
    .osrs_t = BME280_OVERSAMPLING_1,
    .osrs_p = BME280_OVERSAMPLING_1,
    .osrs_h = BME280_OVERSAMPLING_1,
};

static void setup(void)
{
    mock_reset();
    bme280_init(&dev, &mock_i2c, &default_cfg);
}

static void plant_adc(uint32_t adc_p, uint32_t adc_t, uint16_t adc_h)
{
    mock_regs[0xF7] = (uint8_t)(adc_p >> 12);
    mock_regs[0xF8] = (uint8_t)(adc_p >> 4);
    mock_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    mock_regs[0xFA] = (uint8_t)(adc_t >> 12);
    mock_regs[0xFB] = (uint8_t)(adc_t >> 4);
    mock_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);
    mock_regs[0xFD] = (uint8_t)(adc_h >> 8);
    mock_regs[0xFE] = (uint8_t)(adc_h & 0xFF);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME280_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0xD0] = 0x58; /* BMP280 ID, not BME280 */
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME280_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(bme280_init(NULL, &mock_i2c, &default_cfg), BME280_ERR_PARAM);
    bme280_t d;
    ASSERT_EQ(bme280_init(&d, NULL, &default_cfg), BME280_ERR_PARAM);
    ASSERT_EQ(bme280_init(&d, &mock_i2c, NULL), BME280_ERR_PARAM);
    bme280_data_t data;
    ASSERT_EQ(bme280_read(NULL, &data), BME280_ERR_PARAM);
    ASSERT_EQ(bme280_read(&d, NULL), BME280_ERR_PARAM);
    ASSERT_EQ(bme280_reset(NULL), BME280_ERR_PARAM);
    ASSERT_EQ(bme280_set_oversampling(NULL, BME280_OVERSAMPLING_1,
              BME280_OVERSAMPLING_1, BME280_OVERSAMPLING_1), BME280_ERR_PARAM);
}

TEST(test_read_temperature)
{
    setup();
    plant_adc(415148, 519888, 28000);

    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_OK);
    ASSERT_TRUE(data.temperature_c > 15.0f && data.temperature_c < 45.0f);
}

TEST(test_read_pressure)
{
    setup();
    plant_adc(415148, 519888, 28000);

    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_OK);
    ASSERT_TRUE(data.pressure_pa > 10000.0f && data.pressure_pa < 200000.0f);
}

TEST(test_read_humidity)
{
    setup();
    plant_adc(415148, 519888, 28000);

    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_OK);
    /* Humidity should be 0-100% range */
    ASSERT_TRUE(data.humidity_pct >= 0.0f && data.humidity_pct <= 100.0f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_reset)
{
    setup();
    bme280_err_t e = bme280_reset(&dev);
    ASSERT_EQ(e, BME280_OK);
    ASSERT_EQ(mock_regs[0xE0], 0xB6);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bme280_err_t e = bme280_reset(&dev);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_addr_high)
{
    mock_reset();
    bme280_cfg_t cfg = { .addr = BME280_ADDR_HIGH, .osrs_t = BME280_OVERSAMPLING_1,
                         .osrs_p = BME280_OVERSAMPLING_1, .osrs_h = BME280_OVERSAMPLING_1 };
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BME280_OK);
}

TEST(test_oversampling_16x)
{
    mock_reset();
    bme280_cfg_t cfg = { .addr = BME280_ADDR_LOW,
                         .osrs_t = BME280_OVERSAMPLING_16,
                         .osrs_p = BME280_OVERSAMPLING_16,
                         .osrs_h = BME280_OVERSAMPLING_16 };
    bme280_t d;
    bme280_init(&d, &mock_i2c, &cfg);

    plant_adc(415148, 519888, 28000);

    bme280_data_t data;
    bme280_err_t e = bme280_read(&d, &data);
    ASSERT_EQ(e, BME280_OK);
    uint8_t ctrl = mock_last_ctrl_meas;
    ASSERT_EQ((ctrl >> 5) & 0x07, BME280_OVERSAMPLING_16);
    ASSERT_EQ((ctrl >> 2) & 0x07, BME280_OVERSAMPLING_16);
    uint8_t ctrl_hum = mock_regs[0xF2];
    ASSERT_EQ(ctrl_hum & 0x07, BME280_OVERSAMPLING_16);
}

TEST(test_calib_readback)
{
    setup();
    ASSERT_EQ(dev.calib.dig_t1, 27504);
    ASSERT_EQ(dev.calib.dig_t2, 26435);
    ASSERT_EQ(dev.calib.dig_t3, -1000);
    ASSERT_EQ(dev.calib.dig_p1, 36477);
    ASSERT_EQ(dev.calib.dig_h1, 75);
    ASSERT_EQ(dev.calib.dig_h2, 366);
    ASSERT_EQ(dev.calib.dig_h6, 30);
}

TEST(test_calib_i2c_fail)
{
    mock_reset();
    mock_fail_at = 2; /* fail on first bulk read (calibration) */
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_read_timeout)
{
    setup();
    /* Force STATUS register measuring bit (0x08) to stay set forever */
    mock_regs[0xF3] = 0x08;
    plant_adc(415148, 519888, 28000);

    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_ERR_TIMEOUT);
}

TEST(test_set_oversampling)
{
    setup();
    bme280_err_t e = bme280_set_oversampling(&dev, BME280_OVERSAMPLING_4,
                                              BME280_OVERSAMPLING_8,
                                              BME280_OVERSAMPLING_16);
    ASSERT_EQ(e, BME280_OK);
    ASSERT_EQ(dev.cfg.osrs_t, BME280_OVERSAMPLING_4);
    ASSERT_EQ(dev.cfg.osrs_p, BME280_OVERSAMPLING_8);
    ASSERT_EQ(dev.cfg.osrs_h, BME280_OVERSAMPLING_16);
}

TEST(test_set_oversampling_invalid)
{
    setup();
    bme280_err_t e = bme280_set_oversampling(&dev, 6, BME280_OVERSAMPLING_1,
                                              BME280_OVERSAMPLING_1);
    ASSERT_EQ(e, BME280_ERR_PARAM);
    e = bme280_set_oversampling(&dev, BME280_OVERSAMPLING_1, 6,
                                BME280_OVERSAMPLING_1);
    ASSERT_EQ(e, BME280_ERR_PARAM);
    e = bme280_set_oversampling(&dev, BME280_OVERSAMPLING_1,
                                BME280_OVERSAMPLING_1, 6);
    ASSERT_EQ(e, BME280_ERR_PARAM);
}

TEST(test_init_invalid_oversampling)
{
    mock_reset();
    bme280_cfg_t cfg = { .addr = BME280_ADDR_LOW, .osrs_t = 6,
                         .osrs_p = BME280_OVERSAMPLING_1,
                         .osrs_h = BME280_OVERSAMPLING_1 };
    bme280_t d;
    ASSERT_EQ(bme280_init(&d, &mock_i2c, &cfg), BME280_ERR_PARAM);
}

TEST(test_humidity_calib_i2c_fail)
{
    mock_reset();
    mock_fail_at = 3; /* fail on second bulk read (humidity calibration at 0xE1) */
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_init_config_write_fail)
{
    /* init sequence: read_regs(ID)=op0, read_regs(STATUS)=op1,
     * read_regs(CALIB00)=op2, read_regs(CALIB_H2)=op3,
     * write_reg(CONFIG)=op4 → fail. */
    mock_reset();
    mock_fail_at = 4;
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_init_ctrl_hum_write_fail)
{
    /* init sequence: op0..op4 pass, write_reg(CTRL_HUM)=op5 → fail. */
    mock_reset();
    mock_fail_at = 5;
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_read_ctrl_meas_write_fail)
{
    /* After init (6 ops: op0-op5), in bme280_read:
     * write(CTRL_HUM)=op6(ok), write(CTRL_MEAS)=op7 → fail. */
    setup();
    mock_fail_at = 7;
    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_read_status_i2c_fail)
{
    /* After init (op0-op5), in read: write(CTRL_HUM)=op6(ok),
     * write(CTRL_MEAS)=op7(ok), read(STATUS)=op8 → fail. */
    setup();
    mock_fail_at = 8;
    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_read_data_i2c_fail)
{
    /* After init (op0-op5), in read: write(CTRL_HUM)=op6(ok),
     * write(CTRL_MEAS)=op7(ok), read(STATUS)=op8(returns 0x00 so loop breaks),
     * read(PRESS_MSB x8)=op9 → fail. */
    setup();
    mock_regs[0xF3] = 0x00; /* STATUS: not measuring → break immediately */
    mock_fail_at = 9;
    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_ERR_I2C);
}

TEST(test_pressure_var1_zero)
{
    /* dig_p1=0 forces var1==0 in compensate_pressure → returns 0 (pressure=0).
     * Zero out dig_p1 bytes (mock_regs[0x8E..0x8F] = 0) before init. */
    mock_reset();
    mock_regs[0x8E] = 0x00;
    mock_regs[0x8F] = 0x00; /* dig_p1 = 0 */
    bme280_t d;
    bme280_err_t e = bme280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BME280_OK);
    ASSERT_EQ(d.calib.dig_p1, 0);

    plant_adc(415148, 519888, 28000);
    bme280_data_t data;
    e = bme280_read(&d, &data);
    ASSERT_EQ(e, BME280_OK);
    ASSERT_NEAR(data.pressure_pa, 0.0f, 1.0f); /* var1==0 → returns 0 */
}

TEST(test_pressure_large_path)
{
    /* adc_p=0 forces p >= 0x80000000, triggering the large-p branch (p/var1)*2.
     * Verified by calculation: with default calib and adc_p=0, p=0xBACDAA31. */
    setup();
    plant_adc(0, 519888, 28000);
    bme280_data_t data;
    bme280_err_t e = bme280_read(&dev, &data);
    ASSERT_EQ(e, BME280_OK);
    /* pressure will be some large value — just verify no crash and not NaN */
    ASSERT_TRUE(data.pressure_pa == data.pressure_pa);
}

/* ---------- skipped temperature: the extra measurement can fail ---------- */

static const bme280_cfg_t skip_t_cfg = {
    .addr   = BME280_ADDR_LOW,
    .osrs_t = BME280_OVERSAMPLING_SKIP,
    .osrs_p = BME280_OVERSAMPLING_1,
    .osrs_h = BME280_OVERSAMPLING_1,
};

TEST(test_init_skip_temperature_i2c_fail)
{
    /* op0..op4 as in test_init_config_write_fail, then the measurement:
     * write(CTRL_HUM)=op5, write(CTRL_MEAS)=op6, read(STATUS)=op7,
     * read(PRESS_MSB x8)=op8. */
    for (int op = 5; op <= 8; op++)
    {
        mock_reset();
        mock_fail_at = op;
        bme280_t d;
        ASSERT_EQ(bme280_init(&d, &mock_i2c, &skip_t_cfg), BME280_ERR_I2C);
    }
}

TEST(test_init_skip_temperature_timeout)
{
    mock_reset();
    mock_regs[0xF3] = 0x08;   /* measuring stuck at 1 */
    bme280_t d;
    ASSERT_EQ(bme280_init(&d, &mock_i2c, &skip_t_cfg), BME280_ERR_TIMEOUT);
}

/* p.15: forced mode is over when the sensor is back in sleep mode, and Table 21
 * p.28: measuring is 0 during the NVM copy that comes before a conversion. */
TEST(test_read_waits_for_sleep_mode)
{
    setup();
    mock_stays_forced = true;
    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_ERR_TIMEOUT);
}

/* Section 9.1: x1 on all three channels takes 9.3 ms at most, so the delay
 * callback must cover that and may stop soon after. */
TEST(test_read_gives_up_after_the_section_9_1_time)
{
    mock_reset();
    bme280_init(&dev, &mock_delay_i2c, &default_cfg);
    mock_delay_ms_count = 0;
    mock_stays_forced = true;
    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_ERR_TIMEOUT);
    ASSERT_GE(mock_delay_ms_count, 10);
    ASSERT_LE(mock_delay_ms_count, 12);
}

/* Table 21 p.28: "Automatically set to '1' when the NVM data are being copied
 * to image registers". Calibration read before that ends is not calibration. */
TEST(test_init_waits_for_the_nvm_copy)
{
    mock_reset();
    mock_regs[0xF3] = 0x01;
    bme280_t d;
    ASSERT_EQ(bme280_init(&d, &mock_i2c, &default_cfg), BME280_ERR_TIMEOUT);
    ASSERT_EQ(d.calib.dig_t1, 0);
}

/* p.8: the first communication comes 2 ms after power-on. */
TEST(test_init_waits_out_start_up)
{
    mock_reset();
    bme280_t d;
    ASSERT_EQ(bme280_init(&d, &mock_delay_i2c, &default_cfg), BME280_OK);
    ASSERT_GE(mock_delay_ms_count, 1);
}

/* A NACK in the status poll is a sensor that is not up yet. */
TEST(test_init_rides_out_a_nack_in_the_status_poll)
{
    mock_reset();
    mock_fail_at = 1;
    bme280_t d;
    ASSERT_EQ(bme280_init(&d, &mock_i2c, &default_cfg), BME280_OK);
}

/* p.27: 0xB6 runs "the complete power-on-reset procedure". */
TEST(test_reset_waits_for_the_nvm_copy)
{
    setup();
    mock_regs[0xF3] = 0x01;
    ASSERT_EQ(bme280_reset(&dev), BME280_ERR_TIMEOUT);
}

TEST(test_reset_reports_a_sensor_that_stays_silent)
{
    mock_reset();
    bme280_init(&dev, &mock_delay_i2c, &default_cfg);
    mock_fail_at = -1;
    mock_i2c_read_fail = true;
    ASSERT_EQ(bme280_reset(&dev), BME280_ERR_I2C);
    mock_i2c_read_fail = false;
}

TEST(test_read_skip_temperature_measure_fail)
{
    /* Nothing has measured a temperature, so this read must, and that fails */
    setup();
    ASSERT_EQ(bme280_set_oversampling(&dev, BME280_OVERSAMPLING_SKIP,
                                      BME280_OVERSAMPLING_1,
                                      BME280_OVERSAMPLING_1), BME280_OK);
    mock_fail_next = 1;
    bme280_data_t data;
    ASSERT_EQ(bme280_read(&dev, &data), BME280_ERR_I2C);
}

TEST_MAIN("bme280")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_temperature);
    RUN(test_read_pressure);
    RUN(test_read_humidity);
    RUN(test_read_i2c_fail);
    RUN(test_read_waits_for_sleep_mode);
    RUN(test_read_gives_up_after_the_section_9_1_time);
    RUN(test_init_waits_for_the_nvm_copy);
    RUN(test_init_waits_out_start_up);
    RUN(test_init_rides_out_a_nack_in_the_status_poll);
    RUN(test_reset_waits_for_the_nvm_copy);
    RUN(test_reset_reports_a_sensor_that_stays_silent);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_high);
    RUN(test_oversampling_16x);
    RUN(test_calib_readback);
    RUN(test_calib_i2c_fail);
    RUN(test_read_timeout);
    RUN(test_set_oversampling);
    RUN(test_set_oversampling_invalid);
    RUN(test_init_invalid_oversampling);
    RUN(test_humidity_calib_i2c_fail);
    RUN(test_init_config_write_fail);
    RUN(test_init_ctrl_hum_write_fail);
    RUN(test_read_ctrl_meas_write_fail);
    RUN(test_read_status_i2c_fail);
    RUN(test_read_data_i2c_fail);
    RUN(test_pressure_var1_zero);
    RUN(test_pressure_large_path);
    RUN(test_init_skip_temperature_i2c_fail);
    RUN(test_init_skip_temperature_timeout);
    RUN(test_read_skip_temperature_measure_fail);
TEST_MAIN_END()
