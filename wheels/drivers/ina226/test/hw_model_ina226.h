// SPDX-License-Identifier: MIT
/*
 * hw_model_ina226.h — datasheet-accurate 16-bit register model for INA226
 *
 * Models INA226's 16-bit big-endian registers as uint16_t array.
 * Each register address maps to one uint16_t.
 * POR defaults from TI INA226 datasheet (SBOS547C).
 */
#ifndef HW_MODEL_INA226_H
#define HW_MODEL_INA226_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint16_t hw_regs16[HW_REG_COUNT];
static int      hw_write_count;
static int      hw_read_count;

static void hw_model_set_reg16(uint8_t reg, uint16_t val)
{
    hw_regs16[reg] = val;
}

__attribute__((unused))
static uint16_t hw_model_get_reg16(uint8_t reg)
{
    return hw_regs16[reg];
}

static void hw_model_reset(void)
{
    memset(hw_regs16, 0, sizeof(hw_regs16));
    hw_write_count = 0;
    hw_read_count  = 0;
    /* POR defaults from INA226 datasheet */
    hw_model_set_reg16(0x00, 0x4127);   /* Config */
    hw_model_set_reg16(0xFE, 0x5449);   /* Manufacturer ID */
    hw_model_set_reg16(0xFF, 0x2260);   /* Die ID */
}

__attribute__((unused))
static bool hw_model_write_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;
    if (len >= 2) {
        hw_regs16[reg] = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    if (len >= 2) {
        uint16_t val = hw_regs16[reg];
        buf[0] = (uint8_t)(val >> 8);
        buf[1] = (uint8_t)(val & 0xFF);
    }
    return true;
}

/* Inject a known shunt voltage for testing.
 * uv is in microvolts; LSB = 2.5uV */
__attribute__((unused))
static void hw_model_inject_shunt_uv(int32_t uv)
{
    int16_t raw = (int16_t)(uv * 10 / 25);
    hw_model_set_reg16(0x01, (uint16_t)raw);
}

/* Inject a known bus voltage for testing.
 * mv is in millivolts; LSB = 1.25mV */
__attribute__((unused))
static void hw_model_inject_bus_mv(uint32_t mv)
{
    uint16_t raw = (uint16_t)(mv * 100u / 125u);
    hw_model_set_reg16(0x02, raw);
}

#endif /* HW_MODEL_INA226_H */
