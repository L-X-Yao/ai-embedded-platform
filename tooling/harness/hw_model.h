/*
 * hw_model.h — hardware register model for driver tests
 *
 * Header-only. Models a 256-register device with per-register reset
 * values, writable masks, read-only flags, and write callbacks.
 * More realistic than mock_i2c.h for testing register-level driver
 * logic against datasheet behavior.
 *
 * Usage:
 *   #include "hw_model.h"
 *
 *   static hw_reg_model_t model;
 *   hw_model_init(&model);
 *   hw_model_set_reset_value(&model, 0xD0, 0x58);
 *   hw_model_set_readonly(&model, 0xD0);
 *   hw_model_set_on_write(&model, 0xF4, on_ctrl_write);
 *   hw_model_reset(&model);
 *
 *   static const my_driver_i2c_t hal = {
 *       .write_reg = hw_model_i2c_write_reg,
 *       .read_regs = hw_model_i2c_read_regs,
 *       .ctx       = &model,
 *   };
 */

#ifndef HW_MODEL_H
#define HW_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_MODEL_REG_COUNT 256

typedef void (*hw_model_write_cb_t)(void *user, uint8_t reg, uint8_t old_val,
                                    uint8_t new_val);

typedef struct {
    uint8_t             regs[HW_MODEL_REG_COUNT];
    uint8_t             reset_values[HW_MODEL_REG_COUNT];
    uint8_t             writable_mask[HW_MODEL_REG_COUNT];
    hw_model_write_cb_t on_write[HW_MODEL_REG_COUNT];
    void               *user;
    int                 fail_next;
    int                 fail_at;
    int                 op_count;
    uint32_t            delay_ms_total;
} hw_reg_model_t;

static void hw_model_init(hw_reg_model_t *m)
{
    memset(m, 0, sizeof(*m));
    memset(m->writable_mask, 0xFF, sizeof(m->writable_mask));
    m->fail_at = -1;
}

static bool hw_model__should_fail(hw_reg_model_t *m)
{
    int n = m->op_count++;
    if (m->fail_next) { m->fail_next = 0; return true; }
    if (m->fail_at >= 0 && n == m->fail_at) return true;
    return false;
}

static void hw_model_set_reset_value(hw_reg_model_t *m, uint8_t reg,
                                     uint8_t val)
{
    m->reset_values[reg] = val;
}

static void hw_model_set_readonly(hw_reg_model_t *m, uint8_t reg)
{
    m->writable_mask[reg] = 0x00;
}

static void hw_model_set_writable_mask(hw_reg_model_t *m, uint8_t reg,
                                       uint8_t mask)
{
    m->writable_mask[reg] = mask;
}

static void hw_model_set_on_write(hw_reg_model_t *m, uint8_t reg,
                                  hw_model_write_cb_t cb)
{
    m->on_write[reg] = cb;
}

static void hw_model_reset(hw_reg_model_t *m)
{
    memcpy(m->regs, m->reset_values, HW_MODEL_REG_COUNT);
}

/* --- register write (respects masks and callbacks) ----------------------- */

static void hw_model__write_reg(hw_reg_model_t *m, uint8_t reg, uint8_t val)
{
    uint8_t mask = m->writable_mask[reg];
    uint8_t old  = m->regs[reg];
    m->regs[reg] = (old & ~mask) | (val & mask);
    if (m->on_write[reg])
        m->on_write[reg](m->user, reg, old, m->regs[reg]);
}

/* --- I2C HAL callbacks --------------------------------------------------- */

__attribute__((unused))
static bool hw_model_i2c_write_reg(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    if (hw_model__should_fail(m)) return false;
    hw_model__write_reg(m, reg, val);
    return true;
}

__attribute__((unused))
static bool hw_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                   uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    if (hw_model__should_fail(m)) return false;
    uint16_t i;
    for (i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < HW_MODEL_REG_COUNT) ? m->regs[idx] : 0xFF;
    }
    return true;
}

/* --- SPI HAL callbacks --------------------------------------------------- */

__attribute__((unused))
static bool hw_model_spi_write_reg(void *ctx, uint8_t reg, uint8_t val)
{
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    if (hw_model__should_fail(m)) return false;
    hw_model__write_reg(m, reg, val);
    return true;
}

__attribute__((unused))
static bool hw_model_spi_read_regs(void *ctx, uint8_t reg,
                                   uint8_t *buf, uint16_t len)
{
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    if (hw_model__should_fail(m)) return false;
    uint16_t i;
    for (i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < HW_MODEL_REG_COUNT) ? m->regs[idx] : 0xFF;
    }
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    hw_reg_model_t *m = (hw_reg_model_t *)ctx;
    m->delay_ms_total += ms;
}

__attribute__((unused))
static void hw_model_delay_ms_u16(void *ctx, uint16_t ms)
{
    hw_model_delay_ms(ctx, (uint32_t)ms);
}

#endif /* HW_MODEL_H */
