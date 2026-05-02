// SPDX-License-Identifier: MIT
/*
 * mlx90614 — Melexis MLX90614 non-contact infrared temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * SMBus protocol: write command byte, then read 3 bytes (LSB, MSB, PEC).
 * Temperature = raw * 0.02 - 273.15 (Kelvin to Celsius).
 */

#include "mlx90614.h"
#include <string.h>

/* RAM registers */
#define REG_TA      0x06u   /* ambient temperature */
#define REG_TOBJ1   0x07u   /* object 1 temperature */

/* EEPROM registers (access via 0x2X) */
#define REG_EMISSIVITY  0x24u

/* Temperature range: datasheet specifies 0x8000 as error flag */
#define RAW_ERROR_FLAG  0x8000u

static bool smbus_read_word(const mlx90614_t *dev, uint8_t cmd,
                            uint16_t *out)
{
    uint8_t tx = cmd;
    if (!dev->i2c.write(dev->i2c.ctx, dev->cfg.addr, &tx, 1))
        return false;

    uint8_t rx[3]; /* LSB, MSB, PEC (PEC ignored in sim) */
    if (!dev->i2c.read(dev->i2c.ctx, dev->cfg.addr, rx, 3))
        return false;

    *out = (uint16_t)((uint16_t)rx[1] << 8 | rx[0]);
    return true;
}

static bool smbus_write_word(const mlx90614_t *dev, uint8_t cmd,
                             uint16_t val)
{
    uint8_t tx[4];
    tx[0] = cmd;
    tx[1] = (uint8_t)(val & 0xFFu);
    tx[2] = (uint8_t)(val >> 8);
    tx[3] = 0; /* PEC placeholder */
    return dev->i2c.write(dev->i2c.ctx, dev->cfg.addr, tx, 4);
}

static mlx90614_err_t read_temp_reg(mlx90614_t *dev, uint8_t reg,
                                    float *temp_c)
{
    uint16_t raw;
    if (!smbus_read_word(dev, reg, &raw))
        return MLX90614_ERR_I2C;

    if (raw & RAW_ERROR_FLAG)
        return MLX90614_ERR_RANGE;

    *temp_c = (float)raw * 0.02f - 273.15f;
    return MLX90614_OK;
}

mlx90614_err_t mlx90614_init(mlx90614_t *dev, const mlx90614_i2c_t *i2c,
                             const mlx90614_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return MLX90614_ERR_PARAM;
    if (!i2c->write || !i2c->read)
        return MLX90614_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = *i2c;
    dev->cfg = *cfg;

    /* Verify communication by reading ambient temp register */
    uint16_t raw;
    if (!smbus_read_word(dev, REG_TA, &raw))
        return MLX90614_ERR_I2C;

    return MLX90614_OK;
}

mlx90614_err_t mlx90614_read_object_temp(mlx90614_t *dev, float *temp_c)
{
    if (!dev || !temp_c)
        return MLX90614_ERR_PARAM;
    return read_temp_reg(dev, REG_TOBJ1, temp_c);
}

mlx90614_err_t mlx90614_read_ambient_temp(mlx90614_t *dev, float *temp_c)
{
    if (!dev || !temp_c)
        return MLX90614_ERR_PARAM;
    return read_temp_reg(dev, REG_TA, temp_c);
}

mlx90614_err_t mlx90614_read_emissivity(mlx90614_t *dev, float *emissivity)
{
    if (!dev || !emissivity)
        return MLX90614_ERR_PARAM;

    uint16_t raw;
    if (!smbus_read_word(dev, REG_EMISSIVITY, &raw))
        return MLX90614_ERR_I2C;

    *emissivity = (float)raw / 65535.0f;
    return MLX90614_OK;
}

mlx90614_err_t mlx90614_set_emissivity(mlx90614_t *dev, float emissivity)
{
    if (!dev)
        return MLX90614_ERR_PARAM;
    if (emissivity < 0.1f || emissivity > 1.0f)
        return MLX90614_ERR_PARAM;

    uint16_t raw = (uint16_t)(emissivity * 65535.0f + 0.5f);
    if (raw == 0) raw = 1;

    /* Erase first (write 0x0000), wait for EEPROM write cycle, then write new value.
     * Datasheet mandates >= 5 ms between erase and write; use 10 ms for margin. */
    if (!smbus_write_word(dev, REG_EMISSIVITY, 0x0000))
        return MLX90614_ERR_I2C;
    if (dev->i2c.delay_ms)
        dev->i2c.delay_ms(dev->i2c.ctx, 10);
    if (!smbus_write_word(dev, REG_EMISSIVITY, raw))
        return MLX90614_ERR_I2C;

    return MLX90614_OK;
}
