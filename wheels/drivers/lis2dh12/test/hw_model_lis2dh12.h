/*
 * hw_model_lis2dh12.h — LIS2DH12 register model
 *
 * WHO_AM_I (0x0F) = 0x33. 10-bit normal mode, data left-justified in 16 bits.
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

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_regs[0x0F] = 0x33;  /* WHO_AM_I */
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
    uint8_t base = reg & 0x7F;
    for (uint16_t i = 0; i < len; i++)
        buf[i] = hw_regs[(uint8_t)(base + i)];
    return true;
}

static void hw_model_inject_accel(float x_mg, float y_mg, float z_mg,
                                   float sensitivity)
{
    int16_t rx = (int16_t)(x_mg / sensitivity);
    int16_t ry = (int16_t)(y_mg / sensitivity);
    int16_t rz = (int16_t)(z_mg / sensitivity);
    /* Left-justify 10-bit in 16-bit (shift left 6) */
    uint16_t ux = (uint16_t)(rx << 6);
    uint16_t uy = (uint16_t)(ry << 6);
    uint16_t uz = (uint16_t)(rz << 6);
    hw_regs[0x28] = (uint8_t)(ux & 0xFF);
    hw_regs[0x29] = (uint8_t)(ux >> 8);
    hw_regs[0x2A] = (uint8_t)(uy & 0xFF);
    hw_regs[0x2B] = (uint8_t)(uy >> 8);
    hw_regs[0x2C] = (uint8_t)(uz & 0xFF);
    hw_regs[0x2D] = (uint8_t)(uz >> 8);
}

#endif /* HW_MODEL_LIS2DH12_H */
