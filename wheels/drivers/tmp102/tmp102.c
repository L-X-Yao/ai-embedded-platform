// SPDX-License-Identifier: MIT
/*
 * tmp102 — TI TMP102 I2C digital temperature sensor driver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * TMP102 register layout (12-bit mode, default after POR):
 *   0x00 — Temperature  (2 bytes, big-endian, bits[15:4] = raw, 0.0625 C/LSB)
 *   0x01 — Config       (2 bytes, SD bit = byte0 bit0)
 *   0x02 — T_LOW        (2 bytes, same format as temperature)
 *   0x03 — T_HIGH       (2 bytes, same format as temperature)
 */

#include "tmp102.h"
#include "contract.h"
#include <string.h>

#define REG_TEMP    0x00u
#define REG_CONFIG  0x01u
#define REG_TLOW    0x02u
#define REG_THIGH   0x03u

#define CFG_SD_BIT  0x01u   /* shutdown bit in config byte 0 */

/* resolution: 0.0625 C per LSB */
#define LSB_C       0.0625f

static bool read16(const tmp102_t *dev, uint8_t reg, uint8_t *buf)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, 2);
}

static bool write16(const tmp102_t *dev, uint8_t reg,
                    uint8_t msb, uint8_t lsb)
{
    uint8_t buf[2] = { msb, lsb };
    return dev->i2c->write_regs(dev->i2c->ctx, dev->cfg.addr, reg, buf, 2);
}

/* Assemble big-endian pair into 12-bit signed raw (bits[15:4]). */
static int16_t assemble_raw12(uint8_t msb, uint8_t lsb)
{
    uint16_t u16 = (uint16_t)msb << 8 | lsb;
    int16_t raw = (int16_t)u16;
    raw >>= 4;  /* sign-extending arithmetic shift */
    return raw;
}

/* Convert 12-bit raw to Celsius. */
static float raw_to_c(uint8_t msb, uint8_t lsb)
{
    return (float)assemble_raw12(msb, lsb) * LSB_C;
}

/* Convert Celsius to 12-bit register value (bits[15:4]). */
static void c_to_raw(float temp_c, uint8_t *msb, uint8_t *lsb)
{
    int16_t raw = (int16_t)(temp_c / LSB_C);
    uint16_t val = (uint16_t)(raw << 4);
    *msb = (uint8_t)(val >> 8);
    *lsb = (uint8_t)(val & 0xFFu);
}

tmp102_err_t tmp102_init(tmp102_t *dev, const tmp102_i2c_t *i2c,
                         const tmp102_cfg_t *cfg)
{
    if (!dev || !i2c || !cfg)
        return TMP102_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->cfg = *cfg;

    /* Read config register to verify communication. */
    uint8_t buf[2];
    if (!read16(dev, REG_CONFIG, buf))
        return TMP102_ERR_I2C;

    /* Clear shutdown bit to ensure continuous conversion. */
    buf[0] &= (uint8_t)~CFG_SD_BIT;
    if (!write16(dev, REG_CONFIG, buf[0], buf[1]))
        return TMP102_ERR_I2C;

    return TMP102_OK;
}

tmp102_err_t tmp102_read_temp(tmp102_t *dev, float *temp_c)
{
    if (!dev || !temp_c)
        return TMP102_ERR_PARAM;

    uint8_t buf[2];
    if (!read16(dev, REG_TEMP, buf))
        return TMP102_ERR_I2C;

    *temp_c = raw_to_c(buf[0], buf[1]);
    ENSURE(*temp_c == *temp_c);  /* not NaN */
    ENSURE(*temp_c >= -55.0f && *temp_c <= 150.0f);
    return TMP102_OK;
}

static tmp102_err_t write_alert(tmp102_t *dev, uint8_t reg, float temp_c)
{
    if (!dev)
        return TMP102_ERR_PARAM;

    uint8_t msb, lsb;
    c_to_raw(temp_c, &msb, &lsb);

    if (!write16(dev, reg, msb, lsb))
        return TMP102_ERR_I2C;

    return TMP102_OK;
}

tmp102_err_t tmp102_set_alert_high(tmp102_t *dev, float temp_c)
{
    if (!dev)
        return TMP102_ERR_PARAM;
    return write_alert(dev, REG_THIGH, temp_c);
}

tmp102_err_t tmp102_set_alert_low(tmp102_t *dev, float temp_c)
{
    if (!dev)
        return TMP102_ERR_PARAM;
    return write_alert(dev, REG_TLOW, temp_c);
}

static tmp102_err_t set_shutdown_bit(tmp102_t *dev, bool sd)
{
    uint8_t buf[2];
    if (!read16(dev, REG_CONFIG, buf))
        return TMP102_ERR_I2C;

    if (sd)
        buf[0] |= CFG_SD_BIT;
    else
        buf[0] &= (uint8_t)~CFG_SD_BIT;

    if (!write16(dev, REG_CONFIG, buf[0], buf[1]))
        return TMP102_ERR_I2C;

    return TMP102_OK;
}

tmp102_err_t tmp102_shutdown(tmp102_t *dev)
{
    if (!dev)
        return TMP102_ERR_PARAM;
    return set_shutdown_bit(dev, true);
}

tmp102_err_t tmp102_wake(tmp102_t *dev)
{
    if (!dev)
        return TMP102_ERR_PARAM;
    return set_shutdown_bit(dev, false);
}
