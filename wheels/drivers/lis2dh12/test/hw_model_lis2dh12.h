// SPDX-License-Identifier: MIT
/*
 * hw_model_lis2dh12.h — LIS2DH12 register model
 *
 * WHO_AM_I (0x0F) = 0x33. 10-bit normal mode, data left-justified in 16 bits.
 * Table 21 gives the defaults and the register types: writes to type r registers
 * do nothing, and writes to reserved ones are counted because the manual forbids
 * them. The address only moves on with SUB bit 7 set (p.25).
 */
#ifndef HW_MODEL_LIS2DH12_H
#define HW_MODEL_LIS2DH12_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_write_count;
static int     hw_read_count;
static int     hw_reserved_writes;   /* writes the manual forbids, p.32 */

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_reserved_writes = 0;
    hw_regs[0x0F] = 0x33;  /* WHO_AM_I */
    hw_regs[0x1E] = 0x10;  /* CTRL_REG0 */
    hw_regs[0x20] = 0x07;  /* CTRL_REG1 */
}

/* Table 21, p.31 and p.32: the registers of type rw */
static bool hw_model_is_rw(uint8_t reg)
{
    return (reg >= 0x1E && reg <= 0x26) || reg == 0x2E || reg == 0x30 ||
           reg == 0x32 || reg == 0x33 || reg == 0x34 || reg == 0x36 ||
           reg == 0x37 || reg == 0x38 || (reg >= 0x3A && reg <= 0x3F);
}

/* Table 21: the registers of type r */
static bool hw_model_is_r(uint8_t reg)
{
    return reg == 0x07 || reg == 0x0C || reg == 0x0D || reg == 0x0F ||
           (reg >= 0x27 && reg <= 0x2D) || reg == 0x2F || reg == 0x31 ||
           reg == 0x35 || reg == 0x39;
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;

    /* p.25: "the 7 LSb represent the actual register address" */
    uint8_t addr = reg & 0x7F;

    if (hw_model_is_rw(addr))
    {
        hw_regs[addr] = val;
    }
    else if (!hw_model_is_r(addr))
    {
        /* p.32: "must not be changed", and the model leaves them alone */
        hw_reserved_writes++;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    uint8_t base = reg & 0x7F;
    /* p.25: "If the MSb of the SUB field is '1', the SUB (register address) is
     * automatically increased". Without it the same register comes back. */
    bool increment = (reg & 0x80) != 0;
    for (uint16_t i = 0; i < len; i++)
        buf[i] = hw_regs[increment ? (uint8_t)(base + i) : base];
    return true;
}

static void hw_model_inject_accel(float x_mg, float y_mg, float z_mg,
                                   float sensitivity)
{
    int16_t rx = (int16_t)(x_mg / sensitivity);
    int16_t ry = (int16_t)(y_mg / sensitivity);
    int16_t rz = (int16_t)(z_mg / sensitivity);
    /* Left-justify 10-bit in 16-bit (shift left 6) */
    uint16_t ux = (uint16_t)((uint16_t)rx << 6);
    uint16_t uy = (uint16_t)((uint16_t)ry << 6);
    uint16_t uz = (uint16_t)((uint16_t)rz << 6);
    hw_regs[0x28] = (uint8_t)(ux & 0xFF);
    hw_regs[0x29] = (uint8_t)(ux >> 8);
    hw_regs[0x2A] = (uint8_t)(uy & 0xFF);
    hw_regs[0x2B] = (uint8_t)(uy >> 8);
    hw_regs[0x2C] = (uint8_t)(uz & 0xFF);
    hw_regs[0x2D] = (uint8_t)(uz >> 8);
}

#endif /* HW_MODEL_LIS2DH12_H */
