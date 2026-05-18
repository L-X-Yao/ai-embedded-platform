// SPDX-License-Identifier: MIT
/*
 * hw_model_bmm150.h — BMM150 register model
 * Chip ID at 0x40 = 0x32. XY: 13-bit, Z: 15-bit, left-justified.
 */
#ifndef HW_MODEL_BMM150_H
#define HW_MODEL_BMM150_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_write_count;
static int     hw_read_count;

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_regs[0x40] = 0x32;
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;
    hw_regs[reg] = val;
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    for (uint16_t i = 0; i < len; i++)
        buf[i] = hw_regs[(uint8_t)(reg + i)];
    return true;
}

static void hw_model_inject_mag(float x_ut, float y_ut, float z_ut)
{
    int16_t rx = (int16_t)(x_ut / 0.3f);
    int16_t ry = (int16_t)(y_ut / 0.3f);
    int16_t rz = (int16_t)(z_ut / 0.15f);
    uint16_t ux = (uint16_t)((uint16_t)rx << 3);
    uint16_t uy = (uint16_t)((uint16_t)ry << 3);
    uint16_t uz = (uint16_t)((uint16_t)rz << 1);
    hw_regs[0x42] = (uint8_t)(ux & 0xFF);
    hw_regs[0x43] = (uint8_t)(ux >> 8);
    hw_regs[0x44] = (uint8_t)(uy & 0xFF);
    hw_regs[0x45] = (uint8_t)(uy >> 8);
    hw_regs[0x46] = (uint8_t)(uz & 0xFF);
    hw_regs[0x47] = (uint8_t)(uz >> 8);
}

#endif /* HW_MODEL_BMM150_H */
