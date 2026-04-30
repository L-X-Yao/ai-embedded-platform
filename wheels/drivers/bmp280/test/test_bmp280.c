#include "test_harness.h"
#include "test_mock_i2c.h"
#include <math.h>
#include "bmp280.h"

/* ---------- chip-specific mock setup ---------- */

static void mock_reset(void)
{
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

static const bmp280_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
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
    uint8_t ctrl = mock_regs[0xF4];
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
    mock_fail_at = 1;
    bmp280_t d;
    bmp280_err_t e = bmp280_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP280_ERR_I2C);
}

TEST(test_bmp280_err_timeout_triggered)
{
    /* TODO: trigger BMP280_ERR_TIMEOUT and assert return value */
}


TEST_MAIN("bmp280")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_safety);
    RUN(test_read_temperature);
    RUN(test_read_pressure);
    RUN(test_read_i2c_fail);
    RUN(test_reset);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_high);
    RUN(test_oversampling_16x);
    RUN(test_calib_readback);
    RUN(test_calib_i2c_fail);
    RUN(test_bmp280_err_timeout_triggered);
TEST_MAIN_END()

