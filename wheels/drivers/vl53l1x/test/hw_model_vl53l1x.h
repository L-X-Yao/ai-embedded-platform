/*
 * hw_model_vl53l1x.h — datasheet-accurate register model for VL53L1X
 *
 * The VL53L1X uses 16-bit register addresses (up to ~0x0200), so the
 * standard 256-register hw_model.h cannot be used. This file provides
 * a custom 512-byte register array indexed by uint16_t.
 *
 * Modelled registers:
 * - 0x010F: Model ID = 0xEA (read-only)
 * - 0x0110: Module Type = 0xCC (read-only)
 * - 0x0087: System start (0x40 = start, 0x00 = stop)
 * - 0x0096: Result range MSB (read-only, set by measurement engine)
 * - 0x0097: Result range LSB (read-only, set by measurement engine)
 */

#ifndef HW_MODEL_VL53L1X_H
#define HW_MODEL_VL53L1X_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Register address constants (match the driver) */
#define VL1X_REG_SYSTEM_START     0x0087u
#define VL1X_REG_RESULT_RANGE_HI  0x0096u
#define VL1X_REG_RESULT_RANGE_LO  0x0097u
#define VL1X_REG_MODEL_ID         0x010Fu
#define VL1X_REG_MODULE_TYPE      0x0110u

#define VL1X_HW_REG_COUNT  512u

typedef struct {
    uint8_t  regs[VL1X_HW_REG_COUNT];
    uint8_t  reset_values[VL1X_HW_REG_COUNT];
    uint8_t  readonly[VL1X_HW_REG_COUNT];  /* 1 = read-only */
    uint16_t true_range_mm;                 /* simulated distance */
    int      measure_count;
    int      write_count;
    int      read_count;
    bool     fail_next;
} vl53l1x_model_t;

static void vl53l1x_model_reset(vl53l1x_model_t *m)
{
    memcpy(m->regs, m->reset_values, VL1X_HW_REG_COUNT);
    m->measure_count = 0;
    m->write_count   = 0;
    m->read_count    = 0;
    m->fail_next     = false;
}

static void vl53l1x_model_init(vl53l1x_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* POR defaults */
    m->reset_values[VL1X_REG_MODEL_ID]    = 0xEA;
    m->reset_values[VL1X_REG_MODULE_TYPE]  = 0xCC;

    /* Read-only registers */
    m->readonly[VL1X_REG_MODEL_ID]        = 1;
    m->readonly[VL1X_REG_MODULE_TYPE]     = 1;
    m->readonly[VL1X_REG_RESULT_RANGE_HI] = 1;
    m->readonly[VL1X_REG_RESULT_RANGE_LO] = 1;

    m->true_range_mm = 250;  /* default simulated distance */

    vl53l1x_model_reset(m);
}

/* Inject a distance value into the result registers */
static void vl53l1x_model_inject_distance(vl53l1x_model_t *m,
                                           uint16_t dist_mm)
{
    m->true_range_mm = dist_mm;
    m->regs[VL1X_REG_RESULT_RANGE_HI] = (uint8_t)(dist_mm >> 8);
    m->regs[VL1X_REG_RESULT_RANGE_LO] = (uint8_t)(dist_mm & 0xFFu);
}

/* --- I2C HAL callbacks (16-bit register addresses) --- */

__attribute__((unused))
static bool vl53l1x_model_write_reg(void *ctx, uint8_t dev_addr,
                                     uint16_t reg, uint8_t val)
{
    (void)dev_addr;
    vl53l1x_model_t *m = (vl53l1x_model_t *)ctx;
    if (m->fail_next)
    {
        m->fail_next = false;
        return false;
    }
    m->write_count++;

    if (reg >= VL1X_HW_REG_COUNT)
    {
        return true; /* ignore out-of-range writes */
    }

    /* Read-only registers cannot be written by the driver */
    if (m->readonly[reg])
    {
        return true;
    }

    uint8_t old_val = m->regs[reg];
    m->regs[reg] = val;

    /* Side effect: SYSTEM_START write triggers measurement engine */
    if (reg == VL1X_REG_SYSTEM_START && val == 0x40 && old_val != 0x40)
    {
        /* Populate range data from true_range_mm */
        m->regs[VL1X_REG_RESULT_RANGE_HI] =
            (uint8_t)(m->true_range_mm >> 8);
        m->regs[VL1X_REG_RESULT_RANGE_LO] =
            (uint8_t)(m->true_range_mm & 0xFFu);
        m->measure_count++;
    }

    return true;
}

__attribute__((unused))
static bool vl53l1x_model_read_regs(void *ctx, uint8_t dev_addr,
                                     uint16_t reg, uint8_t *buf,
                                     uint16_t len)
{
    (void)dev_addr;
    vl53l1x_model_t *m = (vl53l1x_model_t *)ctx;
    if (m->fail_next)
    {
        m->fail_next = false;
        return false;
    }
    m->read_count++;

    for (uint16_t i = 0; i < len; i++)
    {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < VL1X_HW_REG_COUNT) ? m->regs[idx] : 0xFFu;
    }
    return true;
}

__attribute__((unused))
static void vl53l1x_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    (void)ms;
}

#endif /* HW_MODEL_VL53L1X_H */
