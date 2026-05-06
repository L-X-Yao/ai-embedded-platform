// SPDX-License-Identifier: MIT
/*
 * icm42688 — TDK ICM-42688-P 6-axis IMU driver (SPI)
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "icm42688.h"
#include "contract.h"

/* ---------- SPI protocol ---------- */

/* ICM-42688-P SPI: bit 7 = 1 for read, 0 for write */
#define SPI_READ_BIT  0x80

/* ---------- scale tables ---------- */

/* Accel sensitivity: LSB/g (datasheet Table 2)
 * Index by icm42688_acc_range_t: 16G=0, 8G=1, 4G=2, 2G=3 */
static const float acc_scale[] = {
    2048.0f,    /* +/-16g */
    4096.0f,    /* +/-8g  */
    8192.0f,    /* +/-4g  */
    16384.0f,   /* +/-2g  */
};

/* Gyro sensitivity: LSB/dps (datasheet Table 1)
 * Index by icm42688_gyro_range_t: 2000=0, 1000=1, 500=2, 250=3 */
static const float gyro_scale[] = {
    16.4f,      /* +/-2000 dps */
    32.8f,      /* +/-1000 dps */
    65.5f,      /* +/-500 dps  */
    131.0f,     /* +/-250 dps  */
};

/* ---------- SPI helpers ---------- */

static void reg_write(const icm42688_spi_t *spi, uint8_t reg, uint8_t val)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, reg);          /* bit 7 = 0 → write */
    spi->xfer(spi->ctx, val);
    spi->cs_high(spi->ctx);
}

static uint8_t reg_read(const icm42688_spi_t *spi, uint8_t reg)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, (uint8_t)(reg | SPI_READ_BIT));
    uint8_t val = spi->xfer(spi->ctx, 0x00);
    spi->cs_high(spi->ctx);
    return val;
}

static void reg_read_burst(const icm42688_spi_t *spi, uint8_t reg,
                            uint8_t *buf, uint8_t len)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, (uint8_t)(reg | SPI_READ_BIT));
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = spi->xfer(spi->ctx, 0x00);
    }
    spi->cs_high(spi->ctx);
}

/* ---------- API ---------- */

icm42688_cfg_t icm42688_default_cfg(void)
{
    icm42688_cfg_t c;
    c.acc_range  = ICM42688_ACC_RANGE_8G;
    c.gyro_range = ICM42688_GYRO_RANGE_2000DPS;
    c.acc_odr    = ICM42688_ODR_1KHZ;
    c.gyro_odr   = ICM42688_ODR_1KHZ;
    return c;
}

icm42688_err_t icm42688_init(icm42688_t *dev,
                              const icm42688_spi_t *spi,
                              const icm42688_cfg_t *cfg)
{
    if (!dev || !spi || !cfg) return ICM42688_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return ICM42688_ERR_PARAM;

    /* Validate enum ranges */
    if ((unsigned)cfg->acc_range > 3)  return ICM42688_ERR_PARAM;
    if ((unsigned)cfg->gyro_range > 3) return ICM42688_ERR_PARAM;
    if (cfg->acc_odr < 1 || cfg->acc_odr > 15)  return ICM42688_ERR_PARAM;
    if (cfg->gyro_odr < 1 || cfg->gyro_odr > 15) return ICM42688_ERR_PARAM;

    dev->spi = spi;

    /* Ensure bank 0 is selected */
    reg_write(spi, ICM42688_REG_BANK_SEL, 0x00);

    /* Read and verify WHO_AM_I */
    uint8_t id = reg_read(spi, ICM42688_REG_WHO_AM_I);
    if (id != ICM42688_WHO_AM_I_VAL) return ICM42688_ERR_ID;

    /* Configure gyro: FS_SEL[6:5] | ODR[3:0] */
    uint8_t gyro_cfg = (uint8_t)(((uint8_t)cfg->gyro_range << 5)
                                  | ((uint8_t)cfg->gyro_odr & 0x0F));
    reg_write(spi, ICM42688_REG_GYRO_CONFIG0, gyro_cfg);

    /* Configure accel: FS_SEL[6:5] | ODR[3:0] */
    uint8_t accel_cfg = (uint8_t)(((uint8_t)cfg->acc_range << 5)
                                   | ((uint8_t)cfg->acc_odr & 0x0F));
    reg_write(spi, ICM42688_REG_ACCEL_CONFIG0, accel_cfg);

    /* Enable accel + gyro in low-noise mode: GYRO_MODE[3:2]=11, ACCEL_MODE[1:0]=11 */
    reg_write(spi, ICM42688_REG_PWR_MGMT0, 0x0F);

    /* Store scale divisors for data conversion */
    dev->acc_div  = acc_scale[cfg->acc_range & 0x03];
    dev->gyro_div = gyro_scale[cfg->gyro_range & 0x03];

    INVARIANT(dev->spi != NULL);
    return ICM42688_OK;
}

icm42688_err_t icm42688_read(const icm42688_t *dev,
                              icm42688_data_t *out)
{
    if (!dev || !dev->spi || !out) return ICM42688_ERR_PARAM;

    /* Burst read: TEMP (2) + ACCEL (6) + GYRO (6) = 14 bytes
     * starting from TEMP_DATA1 (0x1D) */
    uint8_t raw[14];
    reg_read_burst(dev->spi, ICM42688_REG_TEMP_DATA1, raw, 14);

    /* Temperature: raw / 132.48 + 25 (datasheet section 14.10) */
    int16_t temp_raw = (int16_t)((uint16_t)raw[0] << 8 | raw[1]);
    out->temperature_c = (float)temp_raw / 132.48f + 25.0f;

    /* Accel: big-endian 16-bit signed, starting at raw[2] */
    int16_t ax = (int16_t)((uint16_t)raw[2] << 8 | raw[3]);
    int16_t ay = (int16_t)((uint16_t)raw[4] << 8 | raw[5]);
    int16_t az = (int16_t)((uint16_t)raw[6] << 8 | raw[7]);

    out->ax_g = (float)ax / dev->acc_div;
    out->ay_g = (float)ay / dev->acc_div;
    out->az_g = (float)az / dev->acc_div;

    /* Gyro: big-endian 16-bit signed, starting at raw[8] */
    int16_t gx = (int16_t)((uint16_t)raw[8]  << 8 | raw[9]);
    int16_t gy = (int16_t)((uint16_t)raw[10] << 8 | raw[11]);
    int16_t gz = (int16_t)((uint16_t)raw[12] << 8 | raw[13]);

    out->gx_dps = (float)gx / dev->gyro_div;
    out->gy_dps = (float)gy / dev->gyro_div;
    out->gz_dps = (float)gz / dev->gyro_div;

    /* NaN guard */
    ENSURE(out->ax_g == out->ax_g);
    ENSURE(out->ay_g == out->ay_g);
    ENSURE(out->az_g == out->az_g);
    ENSURE(out->gx_dps == out->gx_dps);
    ENSURE(out->gy_dps == out->gy_dps);
    ENSURE(out->gz_dps == out->gz_dps);
    ENSURE(out->temperature_c == out->temperature_c);
    return ICM42688_OK;
}

icm42688_err_t icm42688_read_temperature(const icm42688_t *dev,
                                          float *temp_c)
{
    if (!dev || !dev->spi || !temp_c) return ICM42688_ERR_PARAM;

    uint8_t raw[2];
    reg_read_burst(dev->spi, ICM42688_REG_TEMP_DATA1, raw, 2);

    int16_t temp_raw = (int16_t)((uint16_t)raw[0] << 8 | raw[1]);
    *temp_c = (float)temp_raw / 132.48f + 25.0f;

    ENSURE(*temp_c == *temp_c);
    return ICM42688_OK;
}

icm42688_err_t icm42688_set_power_mode(const icm42688_t *dev,
                                        icm42688_power_mode_t accel_mode,
                                        icm42688_power_mode_t gyro_mode)
{
    if (!dev || !dev->spi) return ICM42688_ERR_PARAM;
    if ((unsigned)accel_mode > 3 || (unsigned)gyro_mode > 3)
        return ICM42688_ERR_PARAM;

    /* PWR_MGMT0: GYRO_MODE[3:2] | ACCEL_MODE[1:0] */
    uint8_t pwr = (uint8_t)(((uint8_t)gyro_mode << 2) | (uint8_t)accel_mode);
    reg_write(dev->spi, ICM42688_REG_PWR_MGMT0, pwr);
    return ICM42688_OK;
}

icm42688_err_t icm42688_soft_reset(const icm42688_t *dev)
{
    if (!dev || !dev->spi) return ICM42688_ERR_PARAM;
    /* DEVICE_CONFIG bit 0: soft_reset_config */
    reg_write(dev->spi, ICM42688_REG_DEVICE_CONFIG, 0x01);
    return ICM42688_OK;
}
