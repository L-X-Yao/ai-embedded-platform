// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include <math.h>
#include "bmp388.h"

/* ---------- chip-specific mock setup ---------- */

/*
 * NVM calibration bytes (21 bytes at regs 0x31-0x45).
 * These are realistic values chosen to produce sensible compensation.
 *
 * NVM layout:
 *   [0x31:0x32] NVM_PAR_T1  = 26000 (uint16_t LE) -> par_t1 = 26000 * 2^8
 *   [0x33:0x34] NVM_PAR_T2  = 18000 (uint16_t LE) -> par_t2 = 18000 / 2^30
 *   [0x35]      NVM_PAR_T3  = -10   (int8_t)       -> par_t3 = -10 / 2^48
 *   [0x36:0x37] NVM_PAR_P1  = -4000 (int16_t LE)   -> par_p1 = (-4000 - 2^14) / 2^20
 *   [0x38:0x39] NVM_PAR_P2  = -6000 (int16_t LE)   -> par_p2 = (-6000 - 2^14) / 2^29
 *   [0x3A]      NVM_PAR_P3  = 10    (int8_t)        -> par_p3 = 10 / 2^32
 *   [0x3B]      NVM_PAR_P4  = 0     (int8_t)        -> par_p4 = 0 / 2^37
 *   [0x3C:0x3D] NVM_PAR_P5  = 25000 (uint16_t LE)  -> par_p5 = 25000 * 2^3
 *   [0x3E:0x3F] NVM_PAR_P6  = 28000 (uint16_t LE)  -> par_p6 = 28000 / 2^6
 *   [0x40]      NVM_PAR_P7  = 15    (int8_t)        -> par_p7 = 15 / 2^8
 *   [0x41]      NVM_PAR_P8  = -10   (int8_t)        -> par_p8 = -10 / 2^15
 *   [0x42:0x43] NVM_PAR_P9  = 1000  (int16_t LE)   -> par_p9 = 1000 / 2^48
 *   [0x44]      NVM_PAR_P10 = 10    (int8_t)        -> par_p10 = 10 / 2^48
 *   [0x45]      NVM_PAR_P11 = 0     (int8_t)        -> par_p11 = 0 / 2^65
 */
static void plant_calib(void)
{
    /* NVM_PAR_T1 = 26000 (0x6590) LE */
    mock_regs[0x31] = 0x90; mock_regs[0x32] = 0x65;
    /* NVM_PAR_T2 = 18000 (0x4650) LE */
    mock_regs[0x33] = 0x50; mock_regs[0x34] = 0x46;
    /* NVM_PAR_T3 = -10 (0xF6) */
    mock_regs[0x35] = 0xF6;
    /* NVM_PAR_P1 = -4000 (0xF060) LE */
    mock_regs[0x36] = 0x60; mock_regs[0x37] = 0xF0;
    /* NVM_PAR_P2 = -6000 (0xE890) LE */
    mock_regs[0x38] = 0x90; mock_regs[0x39] = 0xE8;
    /* NVM_PAR_P3 = 10 */
    mock_regs[0x3A] = 0x0A;
    /* NVM_PAR_P4 = 0 */
    mock_regs[0x3B] = 0x00;
    /* NVM_PAR_P5 = 25000 (0x61A8) LE */
    mock_regs[0x3C] = 0xA8; mock_regs[0x3D] = 0x61;
    /* NVM_PAR_P6 = 28000 (0x6D60) LE */
    mock_regs[0x3E] = 0x60; mock_regs[0x3F] = 0x6D;
    /* NVM_PAR_P7 = 15 */
    mock_regs[0x40] = 0x0F;
    /* NVM_PAR_P8 = -10 (0xF6) */
    mock_regs[0x41] = 0xF6;
    /* NVM_PAR_P9 = 1000 (0x03E8) LE */
    mock_regs[0x42] = 0xE8; mock_regs[0x43] = 0x03;
    /* NVM_PAR_P10 = 10 */
    mock_regs[0x44] = 0x0A;
    /* NVM_PAR_P11 = 0 */
    mock_regs[0x45] = 0x00;
}

static void mock_reset(void)
{
    mock_i2c_reset();

    /* Chip ID register */
    mock_regs[0x00] = 0x50;

    /* STATUS: drdy_press set (bit5) so read doesn't timeout */
    mock_regs[0x03] = 0x20;

    plant_calib();
}

static const bmp388_i2c_t mock_i2c = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */
static bmp388_t dev;
static bmp388_cfg_t default_cfg = {
    .addr = BMP388_ADDR_LOW,
    .osrs_t = BMP388_OVERSAMPLING_1,
    .osrs_p = BMP388_OVERSAMPLING_1,
};

static void setup(void)
{
    mock_reset();
    bmp388_init(&dev, &mock_i2c, &default_cfg);
    /* Reset op count after init so read tests start clean */
    mock_op_count = 0;
    mock_fail_at = -1;
    mock_fail_next = 0;
}

/*
 * Plant 24-bit raw ADC values for pressure (DATA_0..2) and temperature (DATA_3..5).
 * BMP388 data is little-endian: XLSB at lower register address.
 */
static void plant_adc(uint32_t raw_press, uint32_t raw_temp)
{
    mock_regs[0x04] = (uint8_t)(raw_press & 0xFF);
    mock_regs[0x05] = (uint8_t)((raw_press >> 8) & 0xFF);
    mock_regs[0x06] = (uint8_t)((raw_press >> 16) & 0xFF);
    mock_regs[0x07] = (uint8_t)(raw_temp & 0xFF);
    mock_regs[0x08] = (uint8_t)((raw_temp >> 8) & 0xFF);
    mock_regs[0x09] = (uint8_t)((raw_temp >> 16) & 0xFF);
}

/* ---------- tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    bmp388_t d;
    bmp388_err_t e = bmp388_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP388_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x00] = 0xFF;
    bmp388_t d;
    bmp388_err_t e = bmp388_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP388_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    bmp388_t d;
    bmp388_err_t e = bmp388_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP388_ERR_I2C);
}

TEST(test_null_dev)
{
    ASSERT_EQ(bmp388_init(NULL, &mock_i2c, &default_cfg), BMP388_ERR_PARAM);
}

TEST(test_null_i2c)
{
    bmp388_t d;
    ASSERT_EQ(bmp388_init(&d, NULL, &default_cfg), BMP388_ERR_PARAM);
}

TEST(test_null_cfg)
{
    bmp388_t d;
    ASSERT_EQ(bmp388_init(&d, &mock_i2c, NULL), BMP388_ERR_PARAM);
}

TEST(test_null_i2c_funcs)
{
    bmp388_i2c_t bad = { .write_reg = NULL, .read_regs = NULL, .ctx = NULL };
    bmp388_t d;
    ASSERT_EQ(bmp388_init(&d, &bad, &default_cfg), BMP388_ERR_PARAM);
}

TEST(test_read_null_dev)
{
    bmp388_data_t data;
    ASSERT_EQ(bmp388_read(NULL, &data), BMP388_ERR_PARAM);
}

TEST(test_read_null_out)
{
    setup();
    ASSERT_EQ(bmp388_read(&dev, NULL), BMP388_ERR_PARAM);
}

TEST(test_reset_null)
{
    ASSERT_EQ(bmp388_reset(NULL), BMP388_ERR_PARAM);
}

TEST(test_read_room_conditions)
{
    setup();

    /*
     * With our calibration data, pick raw values that yield ~25 C / ~101325 Pa.
     * We use trial values and verify they fall in a reasonable range.
     * raw_temp ~ 8000000 -> compensate through par_t1..t3
     * raw_press ~ 6000000 -> compensate through par_p1..p11 + t_lin
     */
    plant_adc(6000000, 8000000);

    bmp388_data_t data;
    bmp388_err_t e = bmp388_read(&dev, &data);
    ASSERT_EQ(e, BMP388_OK);
    /* Verify temperature is in a reasonable range */
    ASSERT_TRUE(data.temperature_c > -40.0f && data.temperature_c < 85.0f);
    /* Verify pressure is in a reasonable range */
    ASSERT_TRUE(data.pressure_pa > 30000.0f && data.pressure_pa < 125000.0f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bmp388_data_t data;
    bmp388_err_t e = bmp388_read(&dev, &data);
    ASSERT_EQ(e, BMP388_ERR_I2C);
}

TEST(test_read_timeout)
{
    setup();
    /* Clear STATUS drdy bit so measurement never becomes ready */
    mock_regs[0x03] = 0x00;
    plant_adc(6000000, 8000000);

    bmp388_data_t data;
    bmp388_err_t e = bmp388_read(&dev, &data);
    ASSERT_EQ(e, BMP388_ERR_TIMEOUT);
}

TEST(test_reset_cmd)
{
    setup();
    bmp388_err_t e = bmp388_reset(&dev);
    ASSERT_EQ(e, BMP388_OK);
    /* CMD register (0x7E) should have received 0xB6 */
    ASSERT_EQ(mock_regs[0x7E], 0xB6);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    bmp388_err_t e = bmp388_reset(&dev);
    ASSERT_EQ(e, BMP388_ERR_I2C);
}

TEST(test_addr_high)
{
    mock_reset();
    bmp388_cfg_t cfg = {
        .addr = BMP388_ADDR_HIGH,
        .osrs_t = BMP388_OVERSAMPLING_1,
        .osrs_p = BMP388_OVERSAMPLING_1,
    };
    bmp388_t d;
    bmp388_err_t e = bmp388_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BMP388_OK);
}

TEST(test_oversampling_config)
{
    mock_reset();
    bmp388_cfg_t cfg = {
        .addr = BMP388_ADDR_LOW,
        .osrs_t = BMP388_OVERSAMPLING_16,
        .osrs_p = BMP388_OVERSAMPLING_8,
    };
    bmp388_t d;
    bmp388_init(&d, &mock_i2c, &cfg);

    /* OSR register 0x1C: osr_t bits[5:3], osr_p bits[2:0].
     * Manual p37: x16 is 100 and x8 is 011 */
    uint8_t osr = mock_regs[0x1C];
    ASSERT_EQ((osr >> 3) & 0x07, 4);
    ASSERT_EQ(osr & 0x07, 3);
}

/* Manual p37: osr_p and osr_t both run from 000 = x1 to 101 = x32 */
TEST(test_oversampling_codes)
{
    static const struct {
        bmp388_oversampling_t os;
        uint8_t               code;
    } v[] = {
        { BMP388_OVERSAMPLING_1,  0 },
        { BMP388_OVERSAMPLING_2,  1 },
        { BMP388_OVERSAMPLING_4,  2 },
        { BMP388_OVERSAMPLING_8,  3 },
        { BMP388_OVERSAMPLING_16, 4 },
        { BMP388_OVERSAMPLING_32, 5 },
    };

    for (unsigned i = 0; i < 6; i++)
    {
        unsigned j = (i + 1) % 6;  /* a different setting in each field */
        mock_reset();
        bmp388_cfg_t cfg = {
            .addr = BMP388_ADDR_LOW,
            .osrs_t = v[i].os,
            .osrs_p = v[j].os,
        };
        bmp388_t d;
        ASSERT_EQ(bmp388_init(&d, &mock_i2c, &cfg), BMP388_OK);
        ASSERT_EQ((mock_regs[0x1C] >> 3) & 0x07, v[i].code);
        ASSERT_EQ(mock_regs[0x1C] & 0x07, v[j].code);
    }
}

/* Nothing above x32 has a code in the manual */
TEST(test_oversampling_above_x32)
{
    mock_reset();
    bmp388_t d;
    bmp388_cfg_t cfg = default_cfg;
    cfg.osrs_t = (bmp388_oversampling_t)7;
    ASSERT_EQ(bmp388_init(&d, &mock_i2c, &cfg), BMP388_ERR_PARAM);
    cfg = default_cfg;
    cfg.osrs_p = (bmp388_oversampling_t)7;
    ASSERT_EQ(bmp388_init(&d, &mock_i2c, &cfg), BMP388_ERR_PARAM);
}

/* OSR has no skip code: p14 skips through press_en and temp_en, which the
 * driver never clears. SKIP keeps measuring at x1, as it always did */
TEST(test_oversampling_skip_measures_x1)
{
    mock_reset();
    bmp388_t d;
    bmp388_cfg_t cfg = default_cfg;
    cfg.osrs_t = BMP388_OVERSAMPLING_SKIP;
    cfg.osrs_p = BMP388_OVERSAMPLING_SKIP;
    ASSERT_EQ(bmp388_init(&d, &mock_i2c, &cfg), BMP388_OK);
    ASSERT_EQ(mock_regs[0x1C], 0x00);

    bmp388_data_t data;
    ASSERT_EQ(bmp388_read(&d, &data), BMP388_OK);
    ASSERT_EQ_HEX(mock_regs[0x1B], 0x13);
}

TEST(test_oversampling_32x)
{
    mock_reset();
    bmp388_cfg_t cfg = {
        .addr = BMP388_ADDR_LOW,
        .osrs_t = BMP388_OVERSAMPLING_32,
        .osrs_p = BMP388_OVERSAMPLING_32,
    };
    bmp388_t d;
    bmp388_err_t e = bmp388_init(&d, &mock_i2c, &cfg);
    ASSERT_EQ(e, BMP388_OK);
    ASSERT_EQ_HEX(mock_regs[0x1C], 0x2D);  /* 101 in both fields, manual p37 */
}

TEST(test_calib_i2c_fail)
{
    mock_reset();
    /* Fail on the second I2C read (calibration read, after chip ID) */
    mock_fail_at = 1;
    bmp388_t d;
    bmp388_err_t e = bmp388_init(&d, &mock_i2c, &default_cfg);
    ASSERT_EQ(e, BMP388_ERR_I2C);
}

TEST(test_forced_mode_write)
{
    setup();
    plant_adc(6000000, 8000000);

    bmp388_data_t data;
    bmp388_read(&dev, &data);

    /* PWR_CTRL register (0x1B) should have forced mode + press/temp enable */
    ASSERT_EQ(mock_regs[0x1B], 0x13);
}

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)BMP388_OK);
    ASSERT_EQ(-1, (int)BMP388_ERR_I2C);
    ASSERT_EQ(-2, (int)BMP388_ERR_ID);
    ASSERT_EQ(-3, (int)BMP388_ERR_PARAM);
    ASSERT_EQ(-4, (int)BMP388_ERR_TIMEOUT);
}

TEST(test_zero_hal_safety)
{
    bmp388_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    bmp388_t d;
    ASSERT_TRUE(bmp388_init(&d, &zero_hal, &default_cfg) < 0);
}

TEST(test_read_data_parsing)
{
    setup();

    /* Plant known raw values and verify they produce valid output */
    plant_adc(5500000, 7500000);

    bmp388_data_t data;
    bmp388_err_t e = bmp388_read(&dev, &data);
    ASSERT_EQ(e, BMP388_OK);
    /* Temperature should be a valid number (not NaN) */
    ASSERT_TRUE(data.temperature_c == data.temperature_c);
    ASSERT_TRUE(data.pressure_pa == data.pressure_pa);
}

TEST_MAIN("bmp388")
    RUN(test_init_ok);
    RUN(test_init_wrong_id);
    RUN(test_init_i2c_fail);
    RUN(test_null_dev);
    RUN(test_null_i2c);
    RUN(test_null_cfg);
    RUN(test_null_i2c_funcs);
    RUN(test_read_null_dev);
    RUN(test_read_null_out);
    RUN(test_reset_null);
    RUN(test_read_room_conditions);
    RUN(test_read_i2c_fail);
    RUN(test_read_timeout);
    RUN(test_reset_cmd);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_high);
    RUN(test_oversampling_config);
    RUN(test_oversampling_codes);
    RUN(test_oversampling_above_x32);
    RUN(test_oversampling_skip_measures_x1);
    RUN(test_oversampling_32x);
    RUN(test_calib_i2c_fail);
    RUN(test_forced_mode_write);
    RUN(test_error_codes);
    RUN(test_zero_hal_safety);
    RUN(test_read_data_parsing);
TEST_MAIN_END()
