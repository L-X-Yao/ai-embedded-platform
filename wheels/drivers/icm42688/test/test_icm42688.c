// SPDX-License-Identifier: MIT
#include "test_harness.h"
#include <string.h>
#include "icm42688.h"

/* ---------- ICM-42688-P register constants for mock ---------- */

#define REG_DEVICE_CONFIG   0x11
#define REG_TEMP_DATA1      0x1D
#define REG_ACCEL_DATA_X1   0x1F
#define REG_GYRO_DATA_X1    0x25
#define REG_GYRO_DATA_Z1    0x29
#define REG_GYRO_CONFIG0    0x4F
#define REG_ACCEL_CONFIG0   0x50
#define REG_PWR_MGMT0       0x4E
#define REG_WHO_AM_I        0x75
#define REG_BANK_SEL        0x76

#define WHO_AM_I_VAL        0x47
#define SPI_READ_BIT        0x80

/* ---------- mock SPI state ---------- */

#define MOCK_REG_COUNT      0x80

static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_cs_state;
static int     mock_in_tx;
static int     mock_byte_idx;
static uint8_t mock_cur_addr;     /* register address for current transaction */

/* xfer log for verifying SPI traffic */
static uint8_t xfer_log[256];
static int     xfer_log_idx;

/* cs_low/cs_high call counts */
static int     mock_cs_low_count;
static int     mock_cs_high_count;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_cs_state = 1;
    mock_in_tx = 0;
    mock_byte_idx = 0;
    mock_cur_addr = 0;
    xfer_log_idx = 0;
    mock_cs_low_count = 0;
    mock_cs_high_count = 0;
    memset(xfer_log, 0, sizeof(xfer_log));

    /* Power-on default: WHO_AM_I = 0x47 */
    mock_regs[REG_WHO_AM_I] = WHO_AM_I_VAL;

    /* Sense registers read -32768 until the first sample (DS-000347 Rev 1.6
     * p.60): high bytes reset to 0x80, low bytes to 0x00 (p.65 to p.68) */
    for (int reg = REG_TEMP_DATA1; reg <= REG_GYRO_DATA_Z1; reg += 2)
        mock_regs[reg] = 0x80;
}

/* First sample arrives on every sensor: 1 g on X at 8 g, everything else 0 */
static void mock_first_sample(void)
{
    memset(&mock_regs[REG_TEMP_DATA1], 0, 14);
    mock_regs[REG_ACCEL_DATA_X1] = 0x10;                /* 0x1000 = 4096 */
}

/* Put one sensor's three axes back to "no sample yet" */
static void mock_no_sample(int first_reg)
{
    for (int i = 0; i < 3; i++) {
        mock_regs[first_reg + 2 * i]     = 0x80;
        mock_regs[first_reg + 2 * i + 1] = 0x00;
    }
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_cs_state = 0;
    mock_in_tx = 1;
    mock_byte_idx = 0;
    mock_cur_addr = 0;
    mock_cs_low_count++;
}

static void mock_cs_high(void *ctx)
{
    (void)ctx;
    mock_cs_state = 1;
    mock_in_tx = 0;
    mock_cs_high_count++;
}

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (xfer_log_idx < (int)sizeof(xfer_log))
        xfer_log[xfer_log_idx++] = tx;

    if (mock_byte_idx == 0) {
        /* First byte: register address + R/W bit */
        mock_cur_addr = (uint8_t)(tx & 0x7F);
        if (tx & SPI_READ_BIT) {
            /* Read: return status byte (0x00) on first byte */
            mock_byte_idx++;
            return 0x00;
        } else {
            /* Write: expect data byte next */
            mock_byte_idx++;
            return 0x00;
        }
    } else {
        /* Subsequent bytes */
        if (xfer_log_idx >= 2 && !(xfer_log[xfer_log_idx - (mock_byte_idx + 1)] & SPI_READ_BIT)) {
            /* Write operation: store value to register */
            uint8_t reg = (uint8_t)(mock_cur_addr + (uint8_t)(mock_byte_idx - 1));
            if (reg < MOCK_REG_COUNT)
                mock_regs[reg] = tx;
            mock_byte_idx++;
            return 0x00;
        } else {
            /* Read operation: return register value, auto-increment */
            uint8_t reg = (uint8_t)(mock_cur_addr + (uint8_t)(mock_byte_idx - 1));
            mock_byte_idx++;
            if (reg < MOCK_REG_COUNT)
                return mock_regs[reg];
            return 0x00;
        }
    }
}

static const icm42688_spi_t mock_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ---------- helper to set up a properly initialized device ---------- */

static icm42688_err_t setup_dev(icm42688_t *dev)
{
    icm42688_cfg_t cfg = icm42688_default_cfg();
    return icm42688_init(dev, &mock_spi, &cfg);
}

/* ---------- tests ---------- */

/* 1. init success */
TEST(test_init_ok)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_err_t e = icm42688_init(&dev, &mock_spi, &cfg);
    ASSERT_EQ(e, ICM42688_OK);
    ASSERT_TRUE(dev.spi == &mock_spi);
}

/* 2. WHO_AM_I mismatch returns ERR_ID */
TEST(test_init_wrong_who_am_i)
{
    mock_reset();
    mock_regs[REG_WHO_AM_I] = 0x00;     /* wrong ID */
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_err_t e = icm42688_init(&dev, &mock_spi, &cfg);
    ASSERT_EQ(e, ICM42688_ERR_ID);
}

/* 3. init writes bank_sel, gyro_config0, accel_config0, pwr_mgmt0 */
TEST(test_init_configures_registers)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_init(&dev, &mock_spi, &cfg);

    /* Check GYRO_CONFIG0: range=2000dps(0)<<5 | ODR=1KHz(6) = 0x06 */
    ASSERT_EQ(mock_regs[REG_GYRO_CONFIG0], 0x06);

    /* Check ACCEL_CONFIG0: range=8G(1)<<5 | ODR=1KHz(6) = 0x26 */
    ASSERT_EQ(mock_regs[REG_ACCEL_CONFIG0], 0x26);

    /* Check PWR_MGMT0: gyro LN(3)<<2 | accel LN(3) = 0x0F */
    ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x0F);

    /* Check BANK_SEL was set to 0 */
    ASSERT_EQ(mock_regs[REG_BANK_SEL], 0x00);
}

/* 4. init with different ranges */
TEST(test_init_ranges_2g_250dps)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg;
    cfg.acc_range  = ICM42688_ACC_RANGE_2G;       /* 3 */
    cfg.gyro_range = ICM42688_GYRO_RANGE_250DPS;  /* 3 */
    cfg.acc_odr    = ICM42688_ODR_100HZ;           /* 8 */
    cfg.gyro_odr   = ICM42688_ODR_200HZ;           /* 7 */
    icm42688_err_t e = icm42688_init(&dev, &mock_spi, &cfg);
    ASSERT_EQ(e, ICM42688_OK);

    /* GYRO_CONFIG0: 3<<5 | 7 = 0x67 */
    ASSERT_EQ(mock_regs[REG_GYRO_CONFIG0], 0x67);
    /* ACCEL_CONFIG0: 3<<5 | 8 = 0x68 */
    ASSERT_EQ(mock_regs[REG_ACCEL_CONFIG0], 0x68);
}

/* 5. read data with known raw values */
TEST(test_read_data_conversion)
{
    mock_reset();
    icm42688_t dev;
    icm42688_err_t e = setup_dev(&dev);
    ASSERT_EQ(e, ICM42688_OK);

    /* Set up known raw sensor data (big-endian) in register map
     * Temp: 0x1D-0x1E, Accel X/Y/Z: 0x1F-0x24, Gyro X/Y/Z: 0x25-0x2A
     *
     * Temperature raw = 0 → 0/132.48 + 25 = 25.0 C
     */
    mock_regs[0x1D] = 0x00; mock_regs[0x1E] = 0x00;   /* temp = 0 */

    /* Accel X = 4096 → 4096/4096.0 = 1.0 G (8G range) */
    mock_regs[0x1F] = 0x10; mock_regs[0x20] = 0x00;   /* 0x1000 = 4096 */

    /* Accel Y = -4096 → -1.0 G */
    mock_regs[0x21] = 0xF0; mock_regs[0x22] = 0x00;   /* 0xF000 = -4096 */

    /* Accel Z = 0 */
    mock_regs[0x23] = 0x00; mock_regs[0x24] = 0x00;

    /* Gyro X = 16 → 16/16.4 ≈ 0.9756 dps (2000dps range) */
    mock_regs[0x25] = 0x00; mock_regs[0x26] = 0x10;   /* 0x0010 = 16 */

    /* Gyro Y = 0 */
    mock_regs[0x27] = 0x00; mock_regs[0x28] = 0x00;

    /* Gyro Z = -164 → -164/16.4 = -10.0 dps */
    mock_regs[0x29] = 0xFF; mock_regs[0x2A] = 0x5C;   /* 0xFF5C = -164 */

    icm42688_data_t data;
    e = icm42688_read(&dev, &data);
    ASSERT_EQ(e, ICM42688_OK);

    ASSERT_NEAR(data.temperature_c, 25.0f, 0.1f);
    ASSERT_NEAR(data.ax_g,  1.0f, 0.01f);
    ASSERT_NEAR(data.ay_g, -1.0f, 0.01f);
    ASSERT_NEAR(data.az_g,  0.0f, 0.01f);
    ASSERT_NEAR(data.gx_dps, 0.9756f, 0.01f);
    ASSERT_NEAR(data.gy_dps, 0.0f, 0.01f);
    ASSERT_NEAR(data.gz_dps, -10.0f, 0.1f);
}

/* 6. read data with 16G range */
TEST(test_read_data_16g_range)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg;
    cfg.acc_range  = ICM42688_ACC_RANGE_16G;
    cfg.gyro_range = ICM42688_GYRO_RANGE_2000DPS;
    cfg.acc_odr    = ICM42688_ODR_1KHZ;
    cfg.gyro_odr   = ICM42688_ODR_1KHZ;
    icm42688_err_t e = icm42688_init(&dev, &mock_spi, &cfg);
    ASSERT_EQ(e, ICM42688_OK);

    /* Accel X = 2048 → 2048/2048.0 = 1.0 G at 16G range */
    mock_regs[0x1D] = 0x00; mock_regs[0x1E] = 0x00;
    mock_regs[0x1F] = 0x08; mock_regs[0x20] = 0x00;   /* 0x0800 = 2048 */
    mock_regs[0x21] = 0x00; mock_regs[0x22] = 0x00;
    mock_regs[0x23] = 0x00; mock_regs[0x24] = 0x00;
    mock_regs[0x25] = 0x00; mock_regs[0x26] = 0x00;
    mock_regs[0x27] = 0x00; mock_regs[0x28] = 0x00;
    mock_regs[0x29] = 0x00; mock_regs[0x2A] = 0x00;

    icm42688_data_t data;
    e = icm42688_read(&dev, &data);
    ASSERT_EQ(e, ICM42688_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);
}

/* 7. read temperature */
TEST(test_read_temperature)
{
    mock_reset();
    icm42688_t dev;
    icm42688_err_t e = setup_dev(&dev);
    ASSERT_EQ(e, ICM42688_OK);

    /* Temp raw = 13248 → 13248/132.48 + 25 = 100 + 25 = 125.0 C */
    mock_regs[0x1D] = 0x33; mock_regs[0x1E] = 0xC0;   /* 0x33C0 = 13248 */

    float temp = 0.0f;
    e = icm42688_read_temperature(&dev, &temp);
    ASSERT_EQ(e, ICM42688_OK);
    ASSERT_NEAR(temp, 125.0f, 0.1f);
}

/* 8. read temperature negative */
TEST(test_read_temperature_negative)
{
    mock_reset();
    icm42688_t dev;
    icm42688_err_t e = setup_dev(&dev);
    ASSERT_EQ(e, ICM42688_OK);

    /* Temp raw = -3312 → -3312/132.48 + 25 = -25 + 25 = 0.0 C */
    int16_t raw_temp = -3312;
    mock_regs[0x1D] = (uint8_t)((uint16_t)raw_temp >> 8);
    mock_regs[0x1E] = (uint8_t)((uint16_t)raw_temp & 0xFF);

    float temp = 99.0f;
    e = icm42688_read_temperature(&dev, &temp);
    ASSERT_EQ(e, ICM42688_OK);
    ASSERT_NEAR(temp, 0.0f, 0.1f);
}

/* 9. power mode setting */
TEST(test_set_power_mode)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.acc_odr = ICM42688_ODR_100HZ;                   /* LP needs <= 500 Hz */
    icm42688_err_t e = icm42688_init(&dev, &mock_spi, &cfg);
    ASSERT_EQ(e, ICM42688_OK);

    /* Set accel LP (2), gyro standby (1) → GYRO_MODE=01<<2 | ACCEL_MODE=10 = 0x06 */
    e = icm42688_set_power_mode(&dev, ICM42688_PM_LP, ICM42688_PM_STANDBY);
    ASSERT_EQ(e, ICM42688_OK);
    ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x06);
}

/* 10. power mode: all off */
TEST(test_set_power_mode_off)
{
    mock_reset();
    icm42688_t dev;
    icm42688_err_t e = setup_dev(&dev);
    ASSERT_EQ(e, ICM42688_OK);

    e = icm42688_set_power_mode(&dev, ICM42688_PM_OFF, ICM42688_PM_OFF);
    ASSERT_EQ(e, ICM42688_OK);
    ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x00);
}

/* 11. soft reset */
TEST(test_soft_reset)
{
    mock_reset();
    icm42688_t dev;
    icm42688_err_t e = setup_dev(&dev);
    ASSERT_EQ(e, ICM42688_OK);

    e = icm42688_soft_reset(&dev);
    ASSERT_EQ(e, ICM42688_OK);
    ASSERT_EQ(mock_regs[REG_DEVICE_CONFIG], 0x01);
}

/* 12. default config values */
TEST(test_default_cfg)
{
    icm42688_cfg_t cfg = icm42688_default_cfg();
    ASSERT_EQ(cfg.acc_range, ICM42688_ACC_RANGE_8G);
    ASSERT_EQ(cfg.gyro_range, ICM42688_GYRO_RANGE_2000DPS);
    ASSERT_EQ(cfg.acc_odr, ICM42688_ODR_1KHZ);
    ASSERT_EQ(cfg.gyro_odr, ICM42688_ODR_1KHZ);
}

/* 13. SPI read uses bit 7 set */
TEST(test_spi_read_bit7)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    xfer_log_idx = 0;
    icm42688_init(&dev, &mock_spi, &cfg);

    /* First SPI transaction: write BANK_SEL (0x76, no read bit)
     * Second SPI transaction: read WHO_AM_I (0x75 | 0x80 = 0xF5) */
    int found_who_am_i_read = 0;
    for (int i = 0; i < xfer_log_idx; i++) {
        if (xfer_log[i] == (REG_WHO_AM_I | SPI_READ_BIT)) {
            found_who_am_i_read = 1;
            break;
        }
    }
    ASSERT_TRUE(found_who_am_i_read);
}

/* 14. error codes have expected values */
TEST(test_error_codes)
{
    ASSERT_EQ( 0, (int)ICM42688_OK);
    ASSERT_EQ(-1, (int)ICM42688_ERR_PARAM);
    ASSERT_EQ(-2, (int)ICM42688_ERR_SPI);
    ASSERT_EQ(-3, (int)ICM42688_ERR_ID);
    ASSERT_EQ(-4, (int)ICM42688_ERR_TIMEOUT);
    ASSERT_EQ(-5, (int)ICM42688_ERR_NOT_READY);
}

/* 15. invalid acc_range rejected */
TEST(test_init_invalid_acc_range)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.acc_range = (icm42688_acc_range_t)99;
    ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_ERR_PARAM);
}

/* 16. invalid gyro_range rejected */
TEST(test_init_invalid_gyro_range)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.gyro_range = (icm42688_gyro_range_t)99;
    ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_ERR_PARAM);
}

/* 17. invalid ODR rejected */
TEST(test_init_invalid_odr)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.acc_odr = (icm42688_odr_t)0;    /* 0 is not valid */
    ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_ERR_PARAM);
}

/* 18. gyro 250dps scale conversion */
TEST(test_gyro_250dps_scale)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg;
    cfg.acc_range  = ICM42688_ACC_RANGE_2G;
    cfg.gyro_range = ICM42688_GYRO_RANGE_250DPS;
    cfg.acc_odr    = ICM42688_ODR_1KHZ;
    cfg.gyro_odr   = ICM42688_ODR_1KHZ;
    ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_OK);

    /* Clear sensor data, set gyro X = 131 → 131/131.0 = 1.0 dps */
    memset(&mock_regs[0x1D], 0, 14);
    mock_regs[0x25] = 0x00; mock_regs[0x26] = 0x83;   /* 0x0083 = 131 */

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.gx_dps, 1.0f, 0.01f);
}

/* 19. null param checks for init */
TEST(test_null_init_dev)
{
    mock_reset();
    icm42688_cfg_t cfg = icm42688_default_cfg();
    ASSERT_EQ(icm42688_init(NULL, &mock_spi, &cfg), ICM42688_ERR_PARAM);
}

TEST(test_null_init_spi)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    ASSERT_EQ(icm42688_init(&dev, NULL, &cfg), ICM42688_ERR_PARAM);
}

TEST(test_null_init_cfg)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(icm42688_init(&dev, &mock_spi, NULL), ICM42688_ERR_PARAM);
}

/* 20. null param checks for read */
TEST(test_null_read_dev)
{
    ASSERT_EQ(icm42688_read(NULL, &(icm42688_data_t){0}), ICM42688_ERR_PARAM);
}

TEST(test_null_read_out)
{
    mock_reset();
    icm42688_t dev;
    setup_dev(&dev);
    ASSERT_EQ(icm42688_read(&dev, NULL), ICM42688_ERR_PARAM);
}

/* 21. null param checks for read_temperature */
TEST(test_null_read_temp_dev)
{
    float t;
    ASSERT_EQ(icm42688_read_temperature(NULL, &t), ICM42688_ERR_PARAM);
}

TEST(test_null_read_temp_out)
{
    mock_reset();
    icm42688_t dev;
    setup_dev(&dev);
    ASSERT_EQ(icm42688_read_temperature(&dev, NULL), ICM42688_ERR_PARAM);
}

/* 22. null param checks for set_power_mode */
TEST(test_null_power_mode)
{
    ASSERT_EQ(icm42688_set_power_mode(NULL, ICM42688_PM_LN, ICM42688_PM_LN),
              ICM42688_ERR_PARAM);
}

/* 23. invalid power mode rejected */
TEST(test_invalid_power_mode)
{
    mock_reset();
    icm42688_t dev;
    setup_dev(&dev);
    ASSERT_EQ(icm42688_set_power_mode(&dev, (icm42688_power_mode_t)99, ICM42688_PM_LN),
              ICM42688_ERR_PARAM);
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LN, (icm42688_power_mode_t)99),
              ICM42688_ERR_PARAM);
}

/* 24. null param checks for soft_reset */
TEST(test_null_soft_reset)
{
    ASSERT_EQ(icm42688_soft_reset(NULL), ICM42688_ERR_PARAM);
}

/* 25. read with uninitialized spi pointer */
TEST(test_read_uninit_spi)
{
    icm42688_t dev;
    memset(&dev, 0, sizeof(dev));
    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_ERR_PARAM);
}

/* 26. SPI HAL function pointer NULL rejected */
TEST(test_init_null_spi_callbacks)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();

    icm42688_spi_t bad_spi = mock_spi;
    bad_spi.cs_low = NULL;
    ASSERT_EQ(icm42688_init(&dev, &bad_spi, &cfg), ICM42688_ERR_PARAM);

    bad_spi = mock_spi;
    bad_spi.cs_high = NULL;
    ASSERT_EQ(icm42688_init(&dev, &bad_spi, &cfg), ICM42688_ERR_PARAM);

    bad_spi = mock_spi;
    bad_spi.xfer = NULL;
    ASSERT_EQ(icm42688_init(&dev, &bad_spi, &cfg), ICM42688_ERR_PARAM);
}

/* 27. CS toggling: each SPI transaction is one cs_low + cs_high pair */
TEST(test_cs_toggling)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_init(&dev, &mock_spi, &cfg);

    /* init does: bank_sel(W) + who_am_i(R) + gyro_cfg(W) + accel_cfg(W) + pwr(W) = 5 transactions */
    ASSERT_EQ(mock_cs_low_count, 5);
    ASSERT_EQ(mock_cs_high_count, 5);
}

/* 28. 2G accel range scale verification */
TEST(test_accel_2g_scale)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg;
    cfg.acc_range  = ICM42688_ACC_RANGE_2G;
    cfg.gyro_range = ICM42688_GYRO_RANGE_2000DPS;
    cfg.acc_odr    = ICM42688_ODR_1KHZ;
    cfg.gyro_odr   = ICM42688_ODR_1KHZ;
    ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_OK);

    /* At 2G range, sensitivity = 16384 LSB/g
     * Raw 16384 → 1.0 G */
    memset(&mock_regs[0x1D], 0, 14);
    mock_regs[0x1F] = 0x40; mock_regs[0x20] = 0x00;   /* 0x4000 = 16384 */

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);
}

/* 29. GYRO_ODR 1100, 1101 and 1110 are Reserved (DS-000347 Rev 1.6 p.77) */
TEST(test_init_refuses_reserved_gyro_odr)
{
    static const uint8_t reserved[] = { 0x0C, 0x0D, 0x0E };

    for (size_t i = 0; i < sizeof(reserved); i++) {
        mock_reset();
        icm42688_t dev;
        icm42688_cfg_t cfg = icm42688_default_cfg();
        cfg.gyro_odr = (icm42688_odr_t)reserved[i];
        ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_ERR_PARAM);
        ASSERT_EQ(mock_regs[REG_GYRO_CONFIG0], 0x00);   /* nothing written */
    }
}

/* 30. every GYRO_ODR code the manual names is still accepted (p.77) */
TEST(test_init_accepts_named_gyro_odr)
{
    static const uint8_t named[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                     0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0F };

    for (size_t i = 0; i < sizeof(named); i++) {
        mock_reset();
        icm42688_t dev;
        icm42688_cfg_t cfg = icm42688_default_cfg();
        cfg.gyro_odr = (icm42688_odr_t)named[i];
        ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_OK);
        ASSERT_EQ(mock_regs[REG_GYRO_CONFIG0], named[i]);   /* 2000 dps = 0 */
    }
}

/* 31. init starts the accel in LN, and 6.25, 3.125 and 1.5625 Hz are LP mode
 * only (DS-000347 Rev 1.6 p.58 and p.78) */
TEST(test_init_refuses_accel_odr_below_ln_range)
{
    static const uint8_t lp_only[] = { 0x0C, 0x0D, 0x0E };

    for (size_t i = 0; i < sizeof(lp_only); i++) {
        mock_reset();
        icm42688_t dev;
        icm42688_cfg_t cfg = icm42688_default_cfg();
        cfg.acc_odr = (icm42688_odr_t)lp_only[i];
        ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_ERR_PARAM);
        ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x00);      /* accel not started */
    }
}

/* 32. every ACCEL_ODR code marked LN or "LP or LN" is still accepted (p.78) */
TEST(test_init_accepts_accel_odr_in_ln_range)
{
    static const uint8_t ln_ok[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                     0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0F };

    for (size_t i = 0; i < sizeof(ln_ok); i++) {
        mock_reset();
        icm42688_t dev;
        icm42688_cfg_t cfg = icm42688_default_cfg();
        cfg.acc_range = ICM42688_ACC_RANGE_16G;         /* FS_SEL = 0 */
        cfg.acc_odr   = (icm42688_odr_t)ln_ok[i];
        ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_OK);
        ASSERT_EQ(mock_regs[REG_ACCEL_CONFIG0], ln_ok[i]);
    }
}

/* 33. GYRO_MODE 10 is Reserved (DS-000347 Rev 1.6 p.76) */
TEST(test_power_mode_refuses_gyro_lp)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);

    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LN, ICM42688_PM_LP),
              ICM42688_ERR_PARAM);
    ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x0F);          /* left as init set it */
}

/* 34. the accel has no standby: ACCEL_MODE 01 "Turns accelerometer off" (p.76) */
TEST(test_power_mode_refuses_accel_standby)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);

    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_STANDBY, ICM42688_PM_LN),
              ICM42688_ERR_PARAM);
    ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x0F);          /* left as init set it */
}

/* 35. gyro standby is real: GYRO_MODE 01 (p.76), with the accel left in LN */
TEST(test_power_mode_accepts_gyro_standby)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);

    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LN, ICM42688_PM_STANDBY),
              ICM42688_OK);
    ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x07);          /* 01 << 2 | 11 */
}

/* 36. accel LP does not support ODR above 500 Hz (DS-000347 Rev 1.6 p.58);
 * codes 0001..0110 are marked "(LN mode)" alone on p.78 */
TEST(test_power_mode_refuses_accel_lp_above_500hz)
{
    static const uint8_t ln_only[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };

    for (size_t i = 0; i < sizeof(ln_only); i++) {
        mock_reset();
        icm42688_t dev;
        icm42688_cfg_t cfg = icm42688_default_cfg();
        cfg.acc_odr = (icm42688_odr_t)ln_only[i];
        ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_OK);

        ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LP, ICM42688_PM_OFF),
                  ICM42688_ERR_PARAM);
        ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x0F);      /* left as init set it */
    }
}

/* 37. the codes marked "(LP or LN mode)" on p.78 may go to LP */
TEST(test_power_mode_accepts_accel_lp_at_500hz_and_below)
{
    static const uint8_t lp_ok[] = { 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0F };

    for (size_t i = 0; i < sizeof(lp_ok); i++) {
        mock_reset();
        icm42688_t dev;
        icm42688_cfg_t cfg = icm42688_default_cfg();
        cfg.acc_odr = (icm42688_odr_t)lp_ok[i];
        ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_OK);

        ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LP, ICM42688_PM_OFF),
                  ICM42688_OK);
        ASSERT_EQ(mock_regs[REG_PWR_MGMT0], 0x02);      /* ACCEL_MODE = 10 */
    }
}

/* 38. the limit follows the chip: ACCEL_CONFIG0 resets to 0x06, which is
 * 1 kHz (p.78), whatever rate init asked for */
TEST(test_power_mode_lp_limit_uses_the_chip_odr)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.acc_odr = ICM42688_ODR_100HZ;
    ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_OK);

    mock_regs[REG_ACCEL_CONFIG0] = 0x06;
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LP, ICM42688_PM_OFF),
              ICM42688_ERR_PARAM);
}

/* 39. every sense register reads -32768 until the first sample (DS-000347
 * Rev 1.6 p.60), which used to come out as about -8 g, -2000 dps and -222 C */
TEST(test_read_before_first_sample)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);

    icm42688_data_t data;
    icm42688_data_t before;
    memset(&data, 0x5A, sizeof(data));
    memcpy(&before, &data, sizeof(data));
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_ERR_NOT_READY);
    ASSERT_MEMEQ_N(&data, &before, sizeof(data));       /* *out left alone */
}

/* 40. the same for the temperature-only read */
TEST(test_read_temperature_before_first_sample)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);

    float temp = 99.0f;
    ASSERT_EQ(icm42688_read_temperature(&dev, &temp), ICM42688_ERR_NOT_READY);
    ASSERT_NEAR(temp, 99.0f, 0.001f);
}

/* 41. the accel starts in 10 ms (p.12) and the gyro in 30 ms (p.11), so the
 * read in between must not pass the gyro's -32768 on as -2000 dps */
TEST(test_read_gyro_still_starting)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);
    mock_first_sample();
    mock_no_sample(REG_GYRO_DATA_X1);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_ERR_NOT_READY);
}

/* 42. the accel side of the same check */
TEST(test_read_accel_still_starting)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);
    mock_first_sample();
    mock_no_sample(REG_ACCEL_DATA_X1);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_ERR_NOT_READY);
}

/* 43. accel LP samples too, so it is waited for like LN */
TEST(test_read_accel_lp_still_starting)
{
    mock_reset();
    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.acc_odr = ICM42688_ODR_100HZ;
    ASSERT_EQ(icm42688_init(&dev, &mock_spi, &cfg), ICM42688_OK);
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LP, ICM42688_PM_OFF),
              ICM42688_OK);
    mock_first_sample();
    mock_no_sample(REG_ACCEL_DATA_X1);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_ERR_NOT_READY);
}

/* 44. the temperature alone still at -32768 */
TEST(test_read_temperature_channel_still_starting)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);
    mock_first_sample();
    mock_regs[REG_TEMP_DATA1] = 0x80;

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_ERR_NOT_READY);
}

/* 45. a disabled sensor may show invalid data for good (p.75, note b), so it
 * must not block the sensor that is on */
TEST(test_read_gyro_off_leaves_accel_usable)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LN, ICM42688_PM_OFF),
              ICM42688_OK);
    mock_first_sample();
    mock_no_sample(REG_GYRO_DATA_X1);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);
}

/* 46. gyro standby keeps the drive on (Table 12, p.25) but gives no samples */
TEST(test_read_gyro_standby_leaves_accel_usable)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_LN, ICM42688_PM_STANDBY),
              ICM42688_OK);
    mock_first_sample();
    mock_no_sample(REG_GYRO_DATA_X1);

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.ax_g, 1.0f, 0.01f);
}

/* 47. the other way round: accel off, gyro on */
TEST(test_read_accel_off_leaves_gyro_usable)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);
    ASSERT_EQ(icm42688_set_power_mode(&dev, ICM42688_PM_OFF, ICM42688_PM_LN),
              ICM42688_OK);
    mock_first_sample();
    mock_no_sample(REG_ACCEL_DATA_X1);
    mock_regs[REG_GYRO_DATA_X1 + 1] = 0x10;             /* 16 LSB */

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.gx_dps, 0.9756f, 0.01f);
}

/* 48. -32766 is the lowest valid sample (p.60), so it is real data */
TEST(test_read_lowest_valid_sample)
{
    mock_reset();
    icm42688_t dev;
    ASSERT_EQ(setup_dev(&dev), ICM42688_OK);
    mock_first_sample();
    mock_regs[REG_ACCEL_DATA_X1]     = 0x80;            /* 0x8002 = -32766 */
    mock_regs[REG_ACCEL_DATA_X1 + 1] = 0x02;

    icm42688_data_t data;
    ASSERT_EQ(icm42688_read(&dev, &data), ICM42688_OK);
    ASSERT_NEAR(data.ax_g, -7.9995f, 0.001f);           /* -32766 / 4096 */
}

TEST_MAIN("icm42688")
    RUN(test_init_ok);
    RUN(test_init_wrong_who_am_i);
    RUN(test_init_configures_registers);
    RUN(test_init_ranges_2g_250dps);
    RUN(test_read_data_conversion);
    RUN(test_read_data_16g_range);
    RUN(test_read_temperature);
    RUN(test_read_temperature_negative);
    RUN(test_set_power_mode);
    RUN(test_set_power_mode_off);
    RUN(test_soft_reset);
    RUN(test_default_cfg);
    RUN(test_spi_read_bit7);
    RUN(test_error_codes);
    RUN(test_init_invalid_acc_range);
    RUN(test_init_invalid_gyro_range);
    RUN(test_init_invalid_odr);
    RUN(test_gyro_250dps_scale);
    RUN(test_null_init_dev);
    RUN(test_null_init_spi);
    RUN(test_null_init_cfg);
    RUN(test_null_read_dev);
    RUN(test_null_read_out);
    RUN(test_null_read_temp_dev);
    RUN(test_null_read_temp_out);
    RUN(test_null_power_mode);
    RUN(test_invalid_power_mode);
    RUN(test_null_soft_reset);
    RUN(test_read_uninit_spi);
    RUN(test_init_null_spi_callbacks);
    RUN(test_cs_toggling);
    RUN(test_accel_2g_scale);
    RUN(test_init_refuses_reserved_gyro_odr);
    RUN(test_init_accepts_named_gyro_odr);
    RUN(test_init_refuses_accel_odr_below_ln_range);
    RUN(test_init_accepts_accel_odr_in_ln_range);
    RUN(test_power_mode_refuses_gyro_lp);
    RUN(test_power_mode_refuses_accel_standby);
    RUN(test_power_mode_accepts_gyro_standby);
    RUN(test_power_mode_refuses_accel_lp_above_500hz);
    RUN(test_power_mode_accepts_accel_lp_at_500hz_and_below);
    RUN(test_power_mode_lp_limit_uses_the_chip_odr);
    RUN(test_read_before_first_sample);
    RUN(test_read_temperature_before_first_sample);
    RUN(test_read_gyro_still_starting);
    RUN(test_read_accel_still_starting);
    RUN(test_read_accel_lp_still_starting);
    RUN(test_read_temperature_channel_still_starting);
    RUN(test_read_gyro_off_leaves_accel_usable);
    RUN(test_read_gyro_standby_leaves_accel_usable);
    RUN(test_read_accel_off_leaves_gyro_usable);
    RUN(test_read_lowest_valid_sample);
TEST_MAIN_END()
