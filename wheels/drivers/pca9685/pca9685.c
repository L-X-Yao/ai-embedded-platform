// SPDX-License-Identifier: MIT
/*
 * pca9685 — I2C 16-channel 12-bit PWM driver (NXP PCA9685)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      i2c_hal (injected)
 * Instances: multi
 *
 * Register map and prescaler formula from NXP PCA9685 datasheet (rev 4).
 * 25 MHz internal oscillator, 12-bit resolution (4096 counts per cycle).
 */

#include "pca9685.h"
#include "contract.h"
#include <string.h>

/* ---- Register addresses ---- */
#define REG_MODE1       0x00u
#define REG_MODE2       0x01u
#define REG_LED0_ON_L   0x06u
#define REG_ALL_ON_L    0xFAu
#define REG_ALL_ON_H    0xFBu
#define REG_ALL_OFF_L   0xFCu
#define REG_ALL_OFF_H   0xFDu
#define REG_PRE_SCALE   0xFEu

/* ---- MODE1 bits ---- */
#define MODE1_RESTART   0x80u
#define MODE1_AI        0x20u   /* auto-increment */
#define MODE1_SLEEP     0x10u
#define MODE1_ALLCALL   0x01u

/* ---- Constants ---- */
#define OSC_CLOCK_HZ    25000000UL
#define COUNT_FULL       4096u
#define FULL_ON_BIT      0x10u  /* bit 4 of ON_H or OFF_H */

/* ---- Internal helpers ---- */

static bool write_reg(const pca9685_t *dev, uint8_t reg, uint8_t val)
{
    return dev->i2c->write_reg(dev->i2c->ctx, dev->dev_addr, reg, val);
}

static bool read_reg(const pca9685_t *dev, uint8_t reg, uint8_t *val)
{
    return dev->i2c->read_regs(dev->i2c->ctx, dev->dev_addr, reg, val, 1);
}

static void delay_ms(const pca9685_t *dev, uint32_t ms)
{
    dev->i2c->delay_ms(dev->i2c->ctx, ms);
}

static uint8_t calc_prescale(uint16_t freq_hz)
{
    /* prescale = round(OSC / (4096 * freq)) - 1 */
    uint32_t denom = (uint32_t)freq_hz * COUNT_FULL;
    uint32_t val = (uint32_t)((OSC_CLOCK_HZ + denom / 2) / denom) - 1u;
    if (val < 3u)   val = 3u;    /* datasheet minimum */
    if (val > 255u) val = 255u;
    return (uint8_t)val;
}

static pca9685_err_t set_prescaler(pca9685_t *dev, uint16_t freq_hz)
{
    uint8_t mode1;
    if (!read_reg(dev, REG_MODE1, &mode1))
        return PCA9685_ERR_I2C;

    /* Must be in sleep mode to write prescaler */
    uint8_t sleep_mode = (uint8_t)((mode1 & ~MODE1_RESTART) | MODE1_SLEEP);
    if (!write_reg(dev, REG_MODE1, sleep_mode))
        return PCA9685_ERR_I2C;

    uint8_t prescale = calc_prescale(freq_hz);
    if (!write_reg(dev, REG_PRE_SCALE, prescale))
        return PCA9685_ERR_I2C;

    /* Wake: clear SLEEP, restore previous mode (without RESTART yet). mode1
     * may hold RESTART=1, and SLEEP must be 0 for 500us before that is written.
     * Init reads mode1 back with SLEEP set, so it has to stay clear below too. */
    uint8_t awake = (uint8_t)(mode1 & ~(MODE1_SLEEP | MODE1_RESTART));
    if (!write_reg(dev, REG_MODE1, awake))
        return PCA9685_ERR_I2C;

    /* Datasheet: 500us stabilization after wake before RESTART */
    delay_ms(dev, 1);

    /* Set RESTART to resume any previously active PWM channels */
    if (!write_reg(dev, REG_MODE1, (uint8_t)(awake | MODE1_RESTART)))
        return PCA9685_ERR_I2C;

    dev->freq_hz = freq_hz;
    return PCA9685_OK;
}

static pca9685_err_t set_full_off(pca9685_t *dev, uint8_t channel)
{
    if (channel >= PCA9685_CHANNELS) return PCA9685_ERR_PARAM;
    uint8_t base = (uint8_t)(REG_LED0_ON_L + channel * 4u);
    if (!write_reg(dev, base,            0))        return PCA9685_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(base + 1u), 0))   return PCA9685_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(base + 2u), 0))   return PCA9685_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(base + 3u), FULL_ON_BIT))
        return PCA9685_ERR_I2C;
    return PCA9685_OK;
}

/* ---- Public API ---- */

pca9685_err_t pca9685_init(pca9685_t *dev, const pca9685_i2c_t *i2c,
                           uint8_t dev_addr, uint16_t freq_hz)
{
    if (!dev || !i2c || !i2c->write_reg || !i2c->read_regs || !i2c->delay_ms)
        return PCA9685_ERR_PARAM;
    if (freq_hz < PCA9685_FREQ_MIN || freq_hz > PCA9685_FREQ_MAX)
        return PCA9685_ERR_PARAM;

    memset(dev, 0, sizeof(*dev));
    dev->i2c      = i2c;
    dev->dev_addr = dev_addr;

    /* MODE1 = SLEEP + AI + ALLCALL. This is no reset, which takes the SWRST
     * General Call: after a restart of the MCU alone, MODE2, the LED registers
     * and EXTCLK keep what they held. */
    uint8_t mode1 = MODE1_SLEEP | MODE1_AI | MODE1_ALLCALL;
    if (!write_reg(dev, REG_MODE1, mode1))
        return PCA9685_ERR_I2C;

    /* Set PWM frequency (also wakes the device) */
    return set_prescaler(dev, freq_hz);
}

pca9685_err_t pca9685_set_pwm(pca9685_t *dev, uint8_t channel,
                              uint16_t on, uint16_t off)
{
    if (!dev) return PCA9685_ERR_PARAM;
    if (channel >= PCA9685_CHANNELS) return PCA9685_ERR_PARAM;
    if (on > PCA9685_COUNT_MAX || off > PCA9685_COUNT_MAX)
        return PCA9685_ERR_PARAM;
    if (on == off)  /* datasheet: never program the same value */
        return PCA9685_ERR_PARAM;

    uint8_t base = (uint8_t)(REG_LED0_ON_L + channel * 4u);

    if (!write_reg(dev, base,            (uint8_t)(on & 0xFFu)))
        return PCA9685_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(base + 1u), (uint8_t)((on >> 8) & 0x0Fu)))
        return PCA9685_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(base + 2u), (uint8_t)(off & 0xFFu)))
        return PCA9685_ERR_I2C;
    if (!write_reg(dev, (uint8_t)(base + 3u), (uint8_t)((off >> 8) & 0x0Fu)))
        return PCA9685_ERR_I2C;

    return PCA9685_OK;
}

pca9685_err_t pca9685_set_duty(pca9685_t *dev, uint8_t channel, float duty_pct)
{
    if (!dev) return PCA9685_ERR_PARAM;
    /* NaN fails both < and > comparisons, so check with negated in-range test */
    if (!(duty_pct >= 0.0f && duty_pct <= 100.0f))
        return PCA9685_ERR_PARAM;

    if (duty_pct <= 0.0f)
        return set_full_off(dev, channel);
    if (duty_pct >= 100.0f) {
        /* Full ON */
        if (channel >= PCA9685_CHANNELS) return PCA9685_ERR_PARAM;
        uint8_t base = (uint8_t)(REG_LED0_ON_L + channel * 4u);
        if (!write_reg(dev, base,            0))             return PCA9685_ERR_I2C;
        if (!write_reg(dev, (uint8_t)(base + 1u), FULL_ON_BIT)) return PCA9685_ERR_I2C;
        if (!write_reg(dev, (uint8_t)(base + 2u), 0))        return PCA9685_ERR_I2C;
        if (!write_reg(dev, (uint8_t)(base + 3u), 0))        return PCA9685_ERR_I2C;
        return PCA9685_OK;
    }

    uint16_t off = (uint16_t)(duty_pct * (float)COUNT_FULL / 100.0f + 0.5f);
    if (off > PCA9685_COUNT_MAX) off = PCA9685_COUNT_MAX;
    if (off == 0u)  /* too short for one count, and would equal ON */
        return set_full_off(dev, channel);
    return pca9685_set_pwm(dev, channel, 0, off);
}

pca9685_err_t pca9685_set_servo_us(pca9685_t *dev, uint8_t channel,
                                   uint16_t pulse_us)
{
    if (!dev) return PCA9685_ERR_PARAM;
    if (dev->freq_hz == 0) return PCA9685_ERR_PARAM;

    /* period_us = 1,000,000 / freq_hz
     * off_count = pulse_us / period_us * 4096 */
    uint32_t period_us = 1000000UL / (uint32_t)dev->freq_hz;
    uint32_t off = (uint32_t)pulse_us * COUNT_FULL / period_us;
    if (off > PCA9685_COUNT_MAX) off = PCA9685_COUNT_MAX;
    uint16_t off16 = (off <= 0xFFFFU) ? (uint16_t)off : 0xFFFFU;

    if (off16 == 0u)  /* too short for one count, and would equal ON */
        return set_full_off(dev, channel);
    return pca9685_set_pwm(dev, channel, 0, off16);
}

pca9685_err_t pca9685_all_off(pca9685_t *dev)
{
    if (!dev) return PCA9685_ERR_PARAM;

    /* ALL_LED registers: set full-off bit */
    if (!write_reg(dev, REG_ALL_ON_L,  0)) return PCA9685_ERR_I2C;
    if (!write_reg(dev, REG_ALL_ON_H,  0)) return PCA9685_ERR_I2C;
    if (!write_reg(dev, REG_ALL_OFF_L, 0)) return PCA9685_ERR_I2C;
    if (!write_reg(dev, REG_ALL_OFF_H, FULL_ON_BIT)) return PCA9685_ERR_I2C;

    return PCA9685_OK;
}

pca9685_err_t pca9685_set_freq(pca9685_t *dev, uint16_t freq_hz)
{
    if (!dev) return PCA9685_ERR_PARAM;
    if (freq_hz < PCA9685_FREQ_MIN || freq_hz > PCA9685_FREQ_MAX)
        return PCA9685_ERR_PARAM;

    return set_prescaler(dev, freq_hz);
}

pca9685_err_t pca9685_sleep(pca9685_t *dev)
{
    if (!dev) return PCA9685_ERR_PARAM;

    uint8_t mode1;
    if (!read_reg(dev, REG_MODE1, &mode1))
        return PCA9685_ERR_I2C;

    if (!write_reg(dev, REG_MODE1, (uint8_t)(mode1 | MODE1_SLEEP)))
        return PCA9685_ERR_I2C;

    return PCA9685_OK;
}

pca9685_err_t pca9685_wake(pca9685_t *dev)
{
    if (!dev) return PCA9685_ERR_PARAM;

    uint8_t mode1;
    if (!read_reg(dev, REG_MODE1, &mode1))
        return PCA9685_ERR_I2C;

    /* Not RESTART yet: SLEEP must be 0 for 500us first */
    uint8_t awake = (uint8_t)(mode1 & ~(MODE1_SLEEP | MODE1_RESTART));
    if (!write_reg(dev, REG_MODE1, awake))
        return PCA9685_ERR_I2C;

    delay_ms(dev, 1);

    /* Set RESTART to resume PWM output */
    if (!write_reg(dev, REG_MODE1, (uint8_t)(awake | MODE1_RESTART)))
        return PCA9685_ERR_I2C;

    return PCA9685_OK;
}
