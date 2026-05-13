/*
 * hw_model_tmp117.h — datasheet-accurate 16-bit register model for TMP117
 *
 * All TMP117 registers are 16-bit, MSB first.
 * Device ID: 0x0117 at register 0x0F.
 * Temperature: signed 16-bit at register 0x00, resolution 0.0078125°C/LSB.
 */
#ifndef HW_MODEL_TMP117_H
#define HW_MODEL_TMP117_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 16

static uint16_t hw_regs16[HW_REG_COUNT];
static int      hw_write_count;
static int      hw_read_count;

static void hw_model_reset(void)
{
    memset(hw_regs16, 0, sizeof(hw_regs16));
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_regs16[0x00] = 0x8000;  /* temperature: POR default */
    hw_regs16[0x01] = 0x0220;  /* config: POR default (continuous, no avg) */
    hw_regs16[0x0F] = 0x0117;  /* device ID */
}

__attribute__((unused))
static bool hw_model_write_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, const uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;
    if (reg >= HW_REG_COUNT || len != 2)
        return false;
    hw_regs16[reg] = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                               uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    if (reg >= HW_REG_COUNT || len != 2)
        return false;
    buf[0] = (uint8_t)(hw_regs16[reg] >> 8);
    buf[1] = (uint8_t)(hw_regs16[reg] & 0xFF);
    return true;
}

static void hw_model_inject_temperature(float temp_c)
{
    int16_t raw = (int16_t)(temp_c / 0.0078125f);
    hw_regs16[0x00] = (uint16_t)raw;
}

#endif /* HW_MODEL_TMP117_H */
