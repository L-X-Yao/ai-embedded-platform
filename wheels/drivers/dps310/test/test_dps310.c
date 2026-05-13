// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include "mock_i2c.h"
#include "dps310.h"

/* ---------- chip-specific mock setup ---------- */

/*
 * DPS310 uses MEAS_CFG (0x08) for both command writes and status reads.
 * Writing a measurement command to bits [2:0] would overwrite the ready
 * flags in bits [6:4] with the raw mock. We wrap write_reg to simulate
 * the sensor's behavior: when a measurement command is written, the
 * ready flags are immediately asserted (simulating instant completion).
 */
static bool dps310_mock_write_reg(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t val)
{
    bool ok = mock_i2c_write_reg(ctx, dev_addr, reg, val);
    if (!ok) { return false; }

    /* Simulate instant measurement completion on MEAS_CFG writes */
    if (reg == 0x08u)
    {
        if (val & 0x02u)  /* temperature command */
        {
            mock_regs[0x08] |= 0x20u;  /* TMP_RDY */
        }
        if (val & 0x01u)  /* pressure command */
        {
            mock_regs[0x08] |= 0x10u;  /* PRS_RDY */
        }
    }
    return true;
}

/*
 * Calibration coefficients in registers 0x10-0x21 (18 bytes).
 * Dummy values chosen for tractable test math:
 *   c0  = 0       (12-bit)
 *   c1  = 100     (12-bit)
 *   c00 = 100000  (20-bit)
 *   c10 = 1000    (20-bit)
 *   c01..c30 = 0  (16-bit each)
 *
 * With 1x oversampling (kT = kP = 524288):
 *   T_comp = c1 * T_raw / kT = 100 * T_raw / 524288
 *   P_comp = c00 + c10 * P_raw / kP = 100000 + 1000 * P_raw / 524288
 */
static void plant_calib(void)
{
    mock_regs[0x10] = 0x00;  /* c0[11:4] */
    mock_regs[0x11] = 0x00;  /* c0[3:0]<<4 | c1[11:8] */
    mock_regs[0x12] = 0x64;  /* c1[7:0] = 100 */
    mock_regs[0x13] = 0x18;  /* c00[19:12] */
    mock_regs[0x14] = 0x6A;  /* c00[11:4] */
    mock_regs[0x15] = 0x00;  /* c00[3:0]<<4 | c10[19:16] */
    mock_regs[0x16] = 0x03;  /* c10[15:8] */
    mock_regs[0x17] = 0xE8;  /* c10[7:0] */
    /* 0x18-0x21 remain zero (c01..c30 = 0) */
}

/*
 * Plant 24-bit raw ADC value in big-endian format.
 * PSR registers: 0x00(MSB) 0x01 0x02(LSB)
 * TMP registers: 0x03(MSB) 0x04 0x05(LSB)
 */
static void plant_tmp(int32_t raw)
{
    uint32_t u = (uint32_t)raw & 0x00FFFFFFu;
    mock_regs[0x03] = (uint8_t)((u >> 16) & 0xFFu);
    mock_regs[0x04] = (uint8_t)((u >> 8) & 0xFFu);
    mock_regs[0x05] = (uint8_t)(u & 0xFFu);
}

static void plant_prs(int32_t raw)
{
    uint32_t u = (uint32_t)raw & 0x00FFFFFFu;
    mock_regs[0x00] = (uint8_t)((u >> 16) & 0xFFu);
    mock_regs[0x01] = (uint8_t)((u >> 8) & 0xFFu);
    mock_regs[0x02] = (uint8_t)(u & 0xFFu);
}

static void mock_reset(void)
{
    mock_i2c_reset();

    /* Product ID register */
    mock_regs[0x0D] = 0x10;

    /* MEAS_CFG: TMP_RDY + PRS_RDY so reads don't timeout */
    mock_regs[0x08] = 0x30;

    plant_calib();
}

static const dps310_i2c_t mock_i2c_hal = {
    .write_reg = dps310_mock_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* For tests that need the raw mock (no MEAS_CFG simulation) */
static const dps310_i2c_t mock_i2c_raw = {
    .write_reg = mock_i2c_write_reg,
    .read_regs = mock_i2c_read_regs,
    .delay_ms  = mock_i2c_delay_ms,
    .ctx       = NULL,
};

/* ---------- test helpers ---------- */

static dps310_t dev;
static dps310_cfg_t default_cfg = {
    .addr   = DPS310_ADDR_HIGH,
    .osrs_t = DPS310_OVERSAMPLING_1,
    .osrs_p = DPS310_OVERSAMPLING_1,
};

static void setup(void)
{
    mock_reset();
    dps310_init(&dev, &mock_i2c_hal, &default_cfg);
    /* Reset op count after init so read tests start clean */
    mock_op_count = 0;
    mock_fail_at = -1;
    mock_fail_next = 0;
}

/* ---------- init tests ---------- */

TEST(test_init_ok)
{
    mock_reset();
    dps310_t d;
    dps310_err_t e = dps310_init(&d, &mock_i2c_hal, &default_cfg);
    ASSERT_EQ(e, DPS310_OK);
}

TEST(test_init_wrong_id)
{
    mock_reset();
    mock_regs[0x0D] = 0xFF;
    dps310_t d;
    dps310_err_t e = dps310_init(&d, &mock_i2c_hal, &default_cfg);
    ASSERT_EQ(e, DPS310_ERR_ID);
}

TEST(test_init_i2c_fail)
{
    mock_reset();
    mock_fail_at = 0;
    dps310_t d;
    dps310_err_t e = dps310_init(&d, &mock_i2c_hal, &default_cfg);
    ASSERT_EQ(e, DPS310_ERR_I2C);
}

TEST(test_null_dev)
{
    ASSERT_EQ(dps310_init(NULL, &mock_i2c_hal, &default_cfg), DPS310_ERR_PARAM);
}

TEST(test_null_i2c)
{
    dps310_t d;
    ASSERT_EQ(dps310_init(&d, NULL, &default_cfg), DPS310_ERR_PARAM);
}

TEST(test_null_cfg)
{
    dps310_t d;
    ASSERT_EQ(dps310_init(&d, &mock_i2c_hal, NULL), DPS310_ERR_PARAM);
}

TEST(test_null_i2c_funcs)
{
    dps310_i2c_t bad = { .write_reg = NULL, .read_regs = NULL, .ctx = NULL };
    dps310_t d;
    ASSERT_EQ(dps310_init(&d, &bad, &default_cfg), DPS310_ERR_PARAM);
}

TEST(test_read_null_dev)
{
    dps310_data_t data;
    ASSERT_EQ(dps310_read(NULL, &data), DPS310_ERR_PARAM);
}

TEST(test_read_null_out)
{
    setup();
    ASSERT_EQ(dps310_read(&dev, NULL), DPS310_ERR_PARAM);
}

TEST(test_reset_null)
{
    ASSERT_EQ(dps310_reset(NULL), DPS310_ERR_PARAM);
}

/* ---------- read tests ---------- */

TEST(test_read_25c_101000pa)
{
    setup();

    /*
     * With dummy calibration (c0=0, c1=100, c00=100000, c10=1000, kT=kP=524288):
     *   T_raw = 25.0 * 524288 / 100 = 131072
     *   T_comp = 100 * 131072 / 524288 = 25.0 C
     *   P_raw = (101000 - 100000) * 524288 / 1000 = 524288
     *   P_comp = 100000 + 1000 * 524288 / 524288 = 101000.0 Pa
     */
    plant_tmp(131072);
    plant_prs(524288);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, 25.0f, 0.5f);
    ASSERT_NEAR(data.pressure_pa, 101000.0f, 50.0f);
}

TEST(test_read_zero_c)
{
    setup();

    /* T_raw = 0 => T_comp = 0 C */
    plant_tmp(0);
    /* P_raw = 0 => P_comp = 100000 Pa */
    plant_prs(0);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, 0.0f, 0.01f);
    ASSERT_NEAR(data.pressure_pa, 100000.0f, 1.0f);
}

TEST(test_read_negative_temp)
{
    setup();

    /*
     * For -10 C: T_raw = -10 * 524288 / 100 = -52428.8 ~ -52429
     * T_comp = 100 * -52429 / 524288 ~ -10.0 C
     */
    plant_tmp(-52429);
    plant_prs(0);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    ASSERT_NEAR(data.temperature_c, -10.0f, 0.1f);
}

TEST(test_read_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_ERR_I2C);
}

/* ---------- reset tests ---------- */

TEST(test_reset_cmd)
{
    setup();
    dps310_err_t e = dps310_reset(&dev);
    ASSERT_EQ(e, DPS310_OK);
    /* RESET register (0x0C) should have received 0x89 */
    ASSERT_EQ(mock_regs[0x0C], 0x89);
}

TEST(test_reset_i2c_fail)
{
    setup();
    mock_fail_next = 1;
    dps310_err_t e = dps310_reset(&dev);
    ASSERT_EQ(e, DPS310_ERR_I2C);
}

/* ---------- address tests ---------- */

TEST(test_addr_low)
{
    mock_reset();
    dps310_cfg_t cfg = {
        .addr   = DPS310_ADDR_LOW,
        .osrs_t = DPS310_OVERSAMPLING_1,
        .osrs_p = DPS310_OVERSAMPLING_1,
    };
    dps310_t d;
    dps310_err_t e = dps310_init(&d, &mock_i2c_hal, &cfg);
    ASSERT_EQ(e, DPS310_OK);
}

/* ---------- oversampling tests ---------- */

TEST(test_oversampling_config)
{
    mock_reset();
    dps310_cfg_t cfg = {
        .addr   = DPS310_ADDR_HIGH,
        .osrs_t = DPS310_OVERSAMPLING_16,
        .osrs_p = DPS310_OVERSAMPLING_8,
    };
    dps310_t d;
    dps310_init(&d, &mock_i2c_hal, &cfg);

    /* PRS_CFG (0x06): bits[2:0] = osrs_p = 3 */
    ASSERT_EQ(mock_regs[0x06] & 0x07, DPS310_OVERSAMPLING_8);
    /* TMP_CFG (0x07): bit7 = TMP_EXT, bits[2:0] = osrs_t = 4 */
    ASSERT_EQ(mock_regs[0x07] & 0x07, DPS310_OVERSAMPLING_16);
    ASSERT_TRUE(mock_regs[0x07] & 0x80);  /* TMP_EXT bit */
}

TEST(test_oversampling_shift_bits)
{
    mock_reset();
    dps310_cfg_t cfg = {
        .addr   = DPS310_ADDR_HIGH,
        .osrs_t = DPS310_OVERSAMPLING_16,
        .osrs_p = DPS310_OVERSAMPLING_32,
    };
    dps310_t d;
    dps310_init(&d, &mock_i2c_hal, &cfg);

    /* CFG_REG (0x09): P_SHIFT=bit2, T_SHIFT=bit3 for >8x oversampling */
    ASSERT_TRUE(mock_regs[0x09] & 0x04);  /* P_SHIFT */
    ASSERT_TRUE(mock_regs[0x09] & 0x08);  /* T_SHIFT */
}

/* ---------- calibration tests ---------- */

TEST(test_calib_i2c_fail)
{
    mock_reset();
    /* Fail on the second I2C read (calibration, after product ID) */
    mock_fail_at = 1;
    dps310_t d;
    dps310_err_t e = dps310_init(&d, &mock_i2c_hal, &default_cfg);
    ASSERT_EQ(e, DPS310_ERR_I2C);
}

/* ---------- mode write tests ---------- */

TEST(test_meas_temp_cmd_written)
{
    setup();
    plant_tmp(131072);
    plant_prs(524288);

    dps310_data_t data;
    dps310_read(&dev, &data);

    /* Verify at least one write happened during read */
    ASSERT_TRUE(mock_write_count > 0);
}

/* ---------- error code tests ---------- */

TEST(test_error_codes)
{
    ASSERT_EQ(0,  (int)DPS310_OK);
    ASSERT_EQ(-1, (int)DPS310_ERR_I2C);
    ASSERT_EQ(-2, (int)DPS310_ERR_ID);
    ASSERT_EQ(-3, (int)DPS310_ERR_PARAM);
}

/* ---------- zero HAL safety ---------- */

TEST(test_zero_hal_safety)
{
    dps310_i2c_t zero_hal;
    memset(&zero_hal, 0, sizeof(zero_hal));
    dps310_t d;
    ASSERT_TRUE(dps310_init(&d, &zero_hal, &default_cfg) < 0);
}

TEST(test_read_data_parsing)
{
    setup();

    /* Plant known raw values and verify valid output */
    plant_tmp(65536);
    plant_prs(262144);

    dps310_data_t data;
    dps310_err_t e = dps310_read(&dev, &data);
    ASSERT_EQ(e, DPS310_OK);
    /* Not NaN */
    ASSERT_TRUE(data.temperature_c == data.temperature_c);
    ASSERT_TRUE(data.pressure_pa == data.pressure_pa);
}

/* ---------- init with raw mock (no MEAS_CFG sim) ---------- */

TEST(test_init_raw_mock)
{
    mock_reset();
    dps310_t d;
    dps310_err_t e = dps310_init(&d, &mock_i2c_raw, &default_cfg);
    ASSERT_EQ(e, DPS310_OK);
}

TEST_MAIN("dps310")
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
    RUN(test_read_25c_101000pa);
    RUN(test_read_zero_c);
    RUN(test_read_negative_temp);
    RUN(test_read_i2c_fail);
    RUN(test_reset_cmd);
    RUN(test_reset_i2c_fail);
    RUN(test_addr_low);
    RUN(test_oversampling_config);
    RUN(test_oversampling_shift_bits);
    RUN(test_calib_i2c_fail);
    RUN(test_meas_temp_cmd_written);
    RUN(test_error_codes);
    RUN(test_zero_hal_safety);
    RUN(test_read_data_parsing);
    RUN(test_init_raw_mock);
TEST_MAIN_END()
