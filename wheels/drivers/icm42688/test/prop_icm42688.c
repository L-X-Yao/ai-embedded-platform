// SPDX-License-Identifier: MIT
/*
 * prop_icm42688.c — property-based tests for icm42688
 *
 * Build: gcc -std=c99 -I.. -I../../../../tooling/harness prop_icm42688.c ../icm42688.c -o prop_icm42688
 */

#include "test_harness.h"
#include "property_test.h"
#include "icm42688.h"
#include <string.h>

/* ---------- mock SPI (same pattern as test_icm42688.c) ---------- */

#define MOCK_REG_COUNT 0x80
#define SPI_READ_BIT   0x80

static uint8_t mock_regs[MOCK_REG_COUNT];
static int     mock_byte_idx;
static uint8_t mock_cur_addr;

static void mock_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_byte_idx = 0;
    mock_cur_addr = 0;
    mock_regs[0x75] = 0x47;   /* WHO_AM_I */
}

static void mock_cs_low(void *ctx)
{
    (void)ctx;
    mock_byte_idx = 0;
    mock_cur_addr = 0;
}

static void mock_cs_high(void *ctx) { (void)ctx; }

static uint8_t mock_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    if (mock_byte_idx == 0) {
        mock_cur_addr = (uint8_t)(tx & 0x7F);
        mock_byte_idx++;
        return 0x00;
    } else {
        if (!(tx == 0x00 || tx == 0xFF)) {
            /* Write operation */
            uint8_t reg = (uint8_t)(mock_cur_addr + (uint8_t)(mock_byte_idx - 1));
            if (reg < MOCK_REG_COUNT)
                mock_regs[reg] = tx;
            mock_byte_idx++;
            return 0x00;
        } else {
            /* Read operation */
            uint8_t reg = (uint8_t)(mock_cur_addr + (uint8_t)(mock_byte_idx - 1));
            mock_byte_idx++;
            return (reg < MOCK_REG_COUNT) ? mock_regs[reg] : 0x00;
        }
    }
}

static const icm42688_spi_t prop_spi = {
    .cs_low  = mock_cs_low,
    .cs_high = mock_cs_high,
    .xfer    = mock_xfer,
    .ctx     = NULL,
};

/* ------------------------------------------------------------------ */

PROPERTY(init_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    /* Fill registers with random data */
    for (int i = 0; i < MOCK_REG_COUNT; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x75] = 0x47;   /* Valid WHO_AM_I */

    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_init(&dev, &prop_spi, &cfg);
    /* Must not crash */
}

PROPERTY(read_no_crash, 10000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    for (int i = 0; i < MOCK_REG_COUNT; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x75] = 0x47;

    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_err_t err = icm42688_init(&dev, &prop_spi, &cfg);
    if (err != ICM42688_OK) return;

    icm42688_data_t data;
    icm42688_read(&dev, &data);
    /* Must not crash with arbitrary sensor data */
}

PROPERTY(accel_finite, 5000)
{
    mock_reset();
    for (int i = 0; i < MOCK_REG_COUNT; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x75] = 0x47;

    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    /* Randomize accel range */
    cfg.acc_range = (icm42688_acc_range_t)(ARBI_UINT8(seed) % 4);

    icm42688_err_t err = icm42688_init(&dev, &prop_spi, &cfg);
    if (err != ICM42688_OK) return;

    icm42688_data_t data;
    err = icm42688_read(&dev, &data);
    if (err != ICM42688_OK) return;

    /* Outputs must not be NaN */
    PROP_ASSERT(data.ax_g == data.ax_g);
    PROP_ASSERT(data.ay_g == data.ay_g);
    PROP_ASSERT(data.az_g == data.az_g);
}

PROPERTY(gyro_finite, 5000)
{
    mock_reset();
    for (int i = 0; i < MOCK_REG_COUNT; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x75] = 0x47;

    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    cfg.gyro_range = (icm42688_gyro_range_t)(ARBI_UINT8(seed) % 4);

    icm42688_err_t err = icm42688_init(&dev, &prop_spi, &cfg);
    if (err != ICM42688_OK) return;

    icm42688_data_t data;
    err = icm42688_read(&dev, &data);
    if (err != ICM42688_OK) return;

    PROP_ASSERT(data.gx_dps == data.gx_dps);
    PROP_ASSERT(data.gy_dps == data.gy_dps);
    PROP_ASSERT(data.gz_dps == data.gz_dps);
}

PROPERTY(temp_finite, 5000)
{
    mock_reset();
    for (int i = 0; i < MOCK_REG_COUNT; i++)
        mock_regs[i] = ARBI_UINT8(seed);
    mock_regs[0x75] = 0x47;

    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_err_t err = icm42688_init(&dev, &prop_spi, &cfg);
    if (err != ICM42688_OK) return;

    float temp = 0.0f;
    err = icm42688_read_temperature(&dev, &temp);
    if (err != ICM42688_OK) return;

    PROP_ASSERT(temp == temp);  /* not NaN */
}

PROPERTY(config_variants, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    mock_regs[0x75] = 0x47;

    icm42688_t dev;
    icm42688_cfg_t cfg;
    cfg.acc_range  = (icm42688_acc_range_t)(ARBI_UINT8(seed) % 4);
    cfg.gyro_range = (icm42688_gyro_range_t)(ARBI_UINT8(seed) % 4);
    cfg.acc_odr    = (icm42688_odr_t)(ARBI_UINT8(seed) % 15 + 1);
    cfg.gyro_odr   = (icm42688_odr_t)(ARBI_UINT8(seed) % 15 + 1);

    icm42688_init(&dev, &prop_spi, &cfg);
    /* Must not crash with any valid config combination */
}

PROPERTY(double_init_safe, 5000)
{
    (void)prop__seed_orig; (void)prop__iter;
    mock_reset();
    mock_regs[0x75] = 0x47;

    icm42688_t dev;
    icm42688_cfg_t cfg = icm42688_default_cfg();
    icm42688_init(&dev, &prop_spi, &cfg);
    mock_reset();
    mock_regs[0x75] = 0x47;
    icm42688_init(&dev, &prop_spi, &cfg);
    /* Must not crash */
}

TEST_MAIN("icm42688 properties")
    RUN(init_no_crash);
    RUN(read_no_crash);
    RUN(accel_finite);
    RUN(gyro_finite);
    RUN(temp_finite);
    RUN(config_variants);
    RUN(double_init_safe);
TEST_MAIN_END()
