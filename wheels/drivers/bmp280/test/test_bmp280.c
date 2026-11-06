// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include <math.h>
#include "bmp280.h"

/* ---------- chip-specific mock setup ---------- */

static bool mock_stays_forced;       /* the measurement never ends */
static bool mock_i2c_read_fail;      /* every read gets no ACK */

static void mock_reset(void)
{
    mock_stays_forced = false;
    mock_i2c_reset();

    mock_regs[0xD0] = 0x58;

    mock_regs[0x88] = 0x70; mock_regs[0x89] = 0x6B;
    mock_regs[0x8A] = 0x43; mock_regs[0x8B] = 0x67;
    mock_regs[0x8C] = 0x18; mock_regs[0x8D] = 0xFC;
    mock_regs[0x8E] = 0x7D; mock_regs[0x8F] = 0x8E;
    mock_regs[0x90] = 0x43; mock_regs[0x91] = 0xD6;
    mock_regs[0x92] = 0xD0; mock_regs[0x93] = 0x0B;
    mock_regs[0x94] = 0x27; mock_regs[0x95] = 0x0B;
    mock_regs[0x96] = 0x8C; mock_regs[0x97] = 0x00;
    mock_regs[0x98] = 0xF9; mock_regs[0x99] = 0xFF;
    mock_regs[0x9A] = 0x8C; mock_regs[0x9B] = 0x3C;
    mock_regs[0x9C] = 0xF8; mock_regs[0x9D] = 0xC6;
    mock_regs[0x9E] = 0x70; mock_regs[0x9F] = 0x17;
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

static const bmp280_i2c_t mock_i2c = {
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
static const bmp280_i2c_t mock_delay_i2c = {
    .write_reg = chip_write_reg,
    .read_regs = chip_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */
static bmp280_t dev;
static bmp280_cfg_t default_cfg = {
    .addr = BMP280_ADDR_LOW,
    .osrs_t = BMP280_OVERSAMPLING_1,
    .osrs_p = BMP280_OVERSAMPLING_1,
};

static void setup(void)
{
    mock_reset();
    bmp280_init(&dev, &mock_i2c, &default_cfg);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0xD0] = 0x00;
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_ERR_I2C);
}

TEST(test_null_safety)
{
    ASSERT_EQ(bmp280_init(NULL, &mock_i2c, &default_cfg), BMP280_ERR_PARAM);
    bmp280_t d;
    ASSERT_EQ(bmp280_init(&d, NULL, &default_cfg), BMP280_ERR_PARAM);
    ASSERT_EQ(bmp280_init(&d, &mock_i2c, NULL), BMP280_ERR_PARAM);
    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(NULL, &data), BMP280_ERR_PARAM);
    ASSERT_EQ(bmp280_read(&d, NULL), BMP280_ERR_PARAM);
    ASSERT_EQ(bmp280_reset(NULL), BMP280_ERR_PARAM);
}

static void plant_adc(uint32_t adc_p, uint32_t adc_t)
{
    mock_regs[0xF7] = (uint8_t)(adc_p >> 12);
    mock_regs[0xF8] = (uint8_t)(adc_p >> 4);
    mock_regs[0xF9] = (uint8_t)((adc_p & 0x0F) << 4);
    mock_regs[0xFA] = (uint8_t)(adc_t >> 12);
    mock_regs[0xFB] = (uint8_t)(adc_t >> 4);
    mock_regs[0xFC] = (uint8_t)((adc_t & 0x0F) << 4);
}

TEST(test_read_temperature)
{
    setup();

    /* adc_t chosen to produce ~25°C with the example calibration data */
    plant_adc(415148, 519888);

    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&dev, &data);
    ASSERT_EQ(e, BMP280_OK);
    ASSERT_TRUE(data.temperature_c > 15.0f && data.temperature_c < 45.0f);
}

TEST(test_read_extreme_calibration_is_defined)
{
    /* Corrupt calibration overflowed the 32-bit formula, which is undefined
     * behaviour. UBSan runs this test, so getting here is the check. */
    static const uint8_t lo_hi[][2] = {{0xFF, 0x7F}, {0x00, 0x80}, {0xFF, 0xFF}};

    for (size_t i = 0; i < sizeof(lo_hi) / sizeof(lo_hi[0]); i++)
    {
        setup();
        for (int reg = 0x88; reg <= 0x9E; reg += 2)
        {
            mock_regs[reg]     = lo_hi[i][0];
            mock_regs[reg + 1] = lo_hi[i][1];
        }
        ASSERT_EQ(bmp280_init(&dev, &mock_i2c, &default_cfg), BMP280_OK);

        static const uint32_t adc[] = {0, 415148, 0xFFFFF};
        for (size_t j = 0; j < sizeof(adc) / sizeof(adc[0]); j++)
        {
            plant_adc(adc[j], adc[j]);
            bmp280_data_t data;
            ASSERT_EQ(bmp280_read(&dev, &data), BMP280_OK);
        }
    }
}

TEST(test_read_pressure)
{
    setup();

    plant_adc(415148, 519888);

    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&dev, &data);
    ASSERT_EQ(e, BMP280_OK);
    ASSERT_TRUE(data.pressure_pa > 10000.0f && data.pressure_pa < 200000.0f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&dev, &data);
    ASSERT_EQ(e, BMP280_ERR_I2C);
}

TEST(test_reset)
{
    setup();
    bmp280_err_t e = bmp280_reset(&dev);
    ASSERT_EQ(e, BMP280_OK);
    ASSERT_EQ(mock_regs[0xE0], 0xB6);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bmp280_err_t e = bmp280_reset(&dev);
    ASSERT_EQ(e, BMP280_ERR_I2C);
}

TEST(test_addr_high)
{
    mock_reset();
    bmp280_cfg_t cfg = { .addr = BMP280_ADDR_HIGH, .osrs_t = BMP280_OVERSAMPLING_1,
                         .osrs_p = BMP280_OVERSAMPLING_1 };
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BMP280_OK);
}

TEST(test_oversampling_16x)
{
    mock_reset();
    bmp280_cfg_t cfg = { .addr = BMP280_ADDR_LOW,
                         .osrs_t = BMP280_OVERSAMPLING_16,
                         .osrs_p = BMP280_OVERSAMPLING_16 };
    bmp280_t d;
    bmp280_init(&d, &mock_i2c, &cfg);

    plant_adc(415148, 519888);

    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&d, &data);
    ASSERT_EQ(e, BMP280_OK);
    uint8_t ctrl = mock_last_ctrl_meas;
    ASSERT_EQ((ctrl >> 5) & 0x07, BMP280_OVERSAMPLING_16);
    ASSERT_EQ((ctrl >> 2) & 0x07, BMP280_OVERSAMPLING_16);
}

TEST(test_calib_readback)
{
    setup();
    ASSERT_EQ(dev.calib.dig_t1, 27504);
    ASSERT_EQ(dev.calib.dig_t2, 26435);
    ASSERT_EQ(dev.calib.dig_t3, -1000);
    ASSERT_EQ(dev.calib.dig_p1, 36477);
}

TEST(test_calib_i2c_fail)
{
    mock_reset();
    mock_fail_at = 2;  /* read(ID)=op0, read(STATUS)=op1, read(CALIB)=op2 */
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_ERR_I2C);
}

TEST(test_read_timeout)
{
    setup();
    /* Force STATUS register measuring bit (0x08) to stay set forever */
    mock_regs[0xF3] = 0x08;
    plant_adc(415148, 519888);

    bmp280_data_t data;
    bmp280_err_t e = bmp280_read(&dev, &data);
    ASSERT_EQ(e, BMP280_ERR_TIMEOUT);
}

/* p.15: forced mode is over when the sensor is back in sleep mode, and Table 19
 * p.25: measuring is 0 during the NVM copy that comes before a conversion. */
TEST(test_read_waits_for_sleep_mode)
{
    setup();
    mock_stays_forced = true;
    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_ERR_TIMEOUT);
}

/* Table 13 p.18: x1 on both channels takes 6.4 ms at most, so the delay callback
 * must cover that and may stop soon after. */
TEST(test_read_gives_up_after_the_table_13_time)
{
    mock_reset();
    bmp280_init(&dev, &mock_delay_i2c, &default_cfg);
    mock_delay_ms_count = 0;
    mock_stays_forced = true;
    bmp280_data_t data;
    ASSERT_EQ(bmp280_read(&dev, &data), BMP280_ERR_TIMEOUT);
    ASSERT_GE(mock_delay_ms_count, 7);
    ASSERT_LE(mock_delay_ms_count, 9);
}

/* Table 19 p.25: "Automatically set to '1' when the NVM data are being copied
 * to image registers". Calibration read before that ends is not calibration. */
TEST(test_init_waits_for_the_nvm_copy)
{
    mock_reset();
    mock_regs[0xF3] = 0x01;
    bmp280_t d;
    ASSERT_EQ(bmp280_init(&d, &mock_i2c, &default_cfg), BMP280_ERR_TIMEOUT);
    ASSERT_EQ(d.calib.dig_t1, 0);
}

/* p.8: the first communication comes 2 ms after power-on. */
TEST(test_init_waits_out_start_up)
{
    mock_reset();
    bmp280_t d;
    ASSERT_EQ(bmp280_init(&d, &mock_delay_i2c, &default_cfg), BMP280_OK);
    ASSERT_GE(mock_delay_ms_count, 1);
}

/* A NACK in the status poll is a sensor that is not up yet. */
TEST(test_init_rides_out_a_nack_in_the_status_poll)
{
    mock_reset();
    mock_fail_at = 1;
    bmp280_t d;
    ASSERT_EQ(bmp280_init(&d, &mock_i2c, &default_cfg), BMP280_OK);
}

/* p.24: 0xB6 runs "the complete power-on-reset procedure". */
TEST(test_reset_waits_for_the_nvm_copy)
{
    setup();
    mock_regs[0xF3] = 0x01;
    ASSERT_EQ(bmp280_reset(&dev), BMP280_ERR_TIMEOUT);
}

TEST(test_reset_reports_a_sensor_that_stays_silent)
{
    mock_reset();
    bmp280_init(&dev, &mock_delay_i2c, &default_cfg);
    mock_i2c_read_fail = true;
    ASSERT_EQ(bmp280_reset(&dev), BMP280_ERR_I2C);
    mock_i2c_read_fail = false;
}

/* ---------- skipped temperature: the extra measurement can fail ---------- */

static const bmp280_cfg_t skip_t_cfg = {
    .addr   = BMP280_ADDR_LOW,
    .osrs_t = BMP280_OVERSAMPLING_SKIP,
    .osrs_p = BMP280_OVERSAMPLING_1,
};

TEST(test_init_skip_temperature_i2c_fail)
{
    /* read(ID)=op0, read(STATUS)=op1, read(CALIB)=op2, write(CONFIG)=op3, then
     * the measurement: write(CTRL_MEAS)=op4, read(STATUS)=op5,
     * read(PRESS_MSB x6)=op6. */
    for (int op = 4; op <= 6; op++)
    {
        mock_reset();
        mock_fail_at = op;
        bmp280_t d;
        ASSERT_EQ(bmp280_init(&d, &mock_i2c, &skip_t_cfg), BMP280_ERR_I2C);
    }
}

TEST(test_init_skip_temperature_timeout)
{
    mock_reset();
    mock_regs[0xF3] = 0x08;   /* measuring stuck at 1 */
    bmp280_t d;
    ASSERT_EQ(bmp280_init(&d, &mock_i2c, &skip_t_cfg), BMP280_ERR_TIMEOUT);
}

TEST_MAIN("bmp280")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_temperature);
    RUN(test_read_extreme_calibration_is_defined);
    RUN(test_read_pressure);
    RUN(test_read_i2c_fail);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_high);
    RUN(test_oversampling_16x);
    RUN(test_calib_readback);
    RUN(test_calib_i2c_fail);
    RUN(test_read_timeout);
    RUN(test_init_waits_for_the_nvm_copy);
    RUN(test_init_waits_out_start_up);
    RUN(test_init_rides_out_a_nack_in_the_status_poll);
    RUN(test_reset_waits_for_the_nvm_copy);
    RUN(test_reset_reports_a_sensor_that_stays_silent);
    RUN(test_read_waits_for_sleep_mode);
    RUN(test_read_gives_up_after_the_table_13_time);
    RUN(test_init_skip_temperature_i2c_fail);
    RUN(test_init_skip_temperature_timeout);
TEST_MAIN_END()

