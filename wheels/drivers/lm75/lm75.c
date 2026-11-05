// SPDX-License-Identifier: MIT
/*
 * lm75 — LM75/LM75A/LM75B I2C digital temperature sensor driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map (LM75 datasheet):
 *   0x00  Temperature (read-only, 2 bytes, 9-bit two's complement in [15:7])
 *   0x01  Configuration (R/W, 1 byte)
 *   0x02  T_hyst (2 bytes, same format as temperature)
 *   0x03  T_os (2 bytes, same format as temperature)
 */

#include "lm75.h"
#include "contract.h"
#include <string.h>

/* ---------- register addresses ---------- */

#define REG_TEMP    0x00u
#define REG_CONF    0x01u
#define REG_THYST   0x02u
#define REG_TOS     0x03u

/* ---------- internal helpers ---------- */

static bool read_regs(const lm75_t *dev, uint8_t reg,
                      uint8_t *buf, uint16_t len)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->addr, reg, buf, len);
}

static bool write_regs(const lm75_t *dev, uint8_t reg,
                       const uint8_t *buf, uint16_t len)
{
    return dev->i2c->write_regs(dev->i2c->ctx, dev->addr, reg, buf, len);
}

/* Convert raw 16-bit register to temperature in degrees C.
 * The 9-bit value sits in bits [15:7], sign-extended, and bits [6:0] are
 * undefined. Temperature = (int16_t)(raw) / 256.0f */
static float raw_to_temp(int16_t raw)
{
    return (float)(int16_t)((uint16_t)raw & 0xFF80u) / 256.0f;
}

/* Convert temperature in degrees C to 9-bit raw register value.
 * Clamp to valid LM75 range: -55 C to +125 C.
 * The 9-bit value is in bits [15:7], lower 7 bits are zero. */
static int16_t temp_to_raw(float temp_c)
{
    if (!(temp_c >= -55.0f && temp_c <= 125.0f)) {
        if (!(temp_c == temp_c)) { return 0; }  /* NaN → 0°C */
        if (temp_c < -55.0f) { temp_c = -55.0f; }
        else { temp_c = 125.0f; }
    }

    /* temp_c in [-55,125] → *256 in [-14080,32000] → fits int16_t */
    int16_t raw16 = (int16_t)(temp_c * 256.0f);
    /* Round to nearest 9-bit boundary (0.5°C step, bit 7 is LSB) */
    raw16 = (int16_t)((raw16 + 64) & ~0x7F);
    return raw16;
}

/* ---------- public API ---------- */

lm75_err_t lm75_init(lm75_t *dev, const lm75_i2c_t *i2c)
{
    return lm75_init_addr(dev, i2c, LM75_DEV_ADDR);
}

lm75_err_t lm75_init_addr(lm75_t *dev, const lm75_i2c_t *i2c, uint8_t addr)
{
    if (!dev || !i2c) {
        return LM75_ERR_PARAM;
    }
    if (!i2c->read_regs || !i2c->write_reg || !i2c->write_regs) {
        return LM75_ERR_PARAM;
    }
    /* Valid LM75 addresses: 0x48 through 0x4F (A0-A2 pins) */
    if (addr < 0x48u || addr > 0x4Fu) {
        return LM75_ERR_PARAM;
    }

    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->addr = addr;

    /* Verify communication by reading the temperature register.
     * LM75 has no chip ID register, so we just confirm the bus works. */
    uint8_t buf[2];
    if (!read_regs(dev, REG_TEMP, buf, 2)) {
        return LM75_ERR_I2C;
    }

    INVARIANT(dev->i2c != NULL);
    return LM75_OK;
}

lm75_err_t lm75_read_temp(lm75_t *dev, float *temp_c)
{
    if (!dev || !temp_c) {
        return LM75_ERR_PARAM;
    }

    uint8_t buf[2];
    if (!read_regs(dev, REG_TEMP, buf, 2)) {
        return LM75_ERR_I2C;
    }

    int16_t raw = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    *temp_c = raw_to_temp(raw);

    ENSURE(*temp_c == *temp_c);  /* not NaN */
    return LM75_OK;
}

lm75_err_t lm75_read_raw(lm75_t *dev, int16_t *raw)
{
    if (!dev || !raw) {
        return LM75_ERR_PARAM;
    }

    uint8_t buf[2];
    if (!read_regs(dev, REG_TEMP, buf, 2)) {
        return LM75_ERR_I2C;
    }

    *raw = (int16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return LM75_OK;
}

lm75_err_t lm75_set_os_temp(lm75_t *dev, float temp_c)
{
    if (!dev) {
        return LM75_ERR_PARAM;
    }

    int16_t raw = temp_to_raw(temp_c);
    uint8_t buf[2];
    buf[0] = (uint8_t)((uint16_t)raw >> 8);
    buf[1] = (uint8_t)((uint16_t)raw & 0xFFu);

    if (!write_regs(dev, REG_TOS, buf, 2)) {
        return LM75_ERR_I2C;
    }

    return LM75_OK;
}

lm75_err_t lm75_set_hyst_temp(lm75_t *dev, float temp_c)
{
    if (!dev) {
        return LM75_ERR_PARAM;
    }

    int16_t raw = temp_to_raw(temp_c);
    uint8_t buf[2];
    buf[0] = (uint8_t)((uint16_t)raw >> 8);
    buf[1] = (uint8_t)((uint16_t)raw & 0xFFu);

    if (!write_regs(dev, REG_THYST, buf, 2)) {
        return LM75_ERR_I2C;
    }

    return LM75_OK;
}
