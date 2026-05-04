/*
 * hw_model_at24cxx.h -- AT24CXX I2C EEPROM hardware model
 *
 * Models AT24CXX EEPROM behavior:
 * - Byte storage (default 256 bytes for AT24C02)
 * - Page write boundaries (8 bytes for C01/C02)
 * - Read at any address
 * - Write-cycle delay (simulated, tracked by counter)
 * - 8-bit address chips (C01-C16) with address in I2C device addr bits
 *
 * Uses the hw_reg_model_t as a 256-byte EEPROM for AT24C02.
 * For simplicity, models only the 8-bit address variant.
 */

#ifndef HW_MODEL_AT24CXX_H
#define HW_MODEL_AT24CXX_H

#include "hw_model.h"

#define AT24CXX_MODEL_SIZE  256   /* AT24C02: 256 bytes */

typedef struct {
    hw_reg_model_t hw;         /* regs[0..255] = EEPROM contents */
    int            write_count;
    int            delay_count;
} at24cxx_hw_model_t;

static void at24cxx_hw_model_init(at24cxx_hw_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* EEPROM starts erased (0xFF) */
    memset(m->hw.reset_values, 0xFF, AT24CXX_MODEL_SIZE);
    hw_model_reset(&m->hw);
}

/*
 * I2C write_reg callback (8-bit address, single byte).
 * Maps directly to hw_model register write.
 */
__attribute__((unused))
static bool at24cxx_model_i2c_write_reg(void *ctx, uint8_t dev_addr,
                                        uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    m->hw.regs[reg] = val;
    m->write_count++;
    return true;
}

/*
 * I2C write_regs8 callback (8-bit address, multi-byte page write).
 */
__attribute__((unused))
static bool at24cxx_model_i2c_write_regs8(void *ctx, uint8_t dev_addr,
                                          uint8_t reg,
                                          const uint8_t *data, uint16_t len)
{
    (void)dev_addr;
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t addr = (uint8_t)((uint16_t)reg + i);
        m->hw.regs[addr] = data[i];
    }
    m->write_count++;
    return true;
}

/*
 * I2C read_regs callback (8-bit address, sequential read).
 */
__attribute__((unused))
static bool at24cxx_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                        uint8_t reg,
                                        uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < AT24CXX_MODEL_SIZE) ? m->hw.regs[idx] : 0xFF;
    }
    return true;
}

/* Delay callback: just count calls */
__attribute__((unused))
static void at24cxx_model_delay(void *ctx, uint32_t ms)
{
    (void)ms;
    at24cxx_hw_model_t *m = (at24cxx_hw_model_t *)ctx;
    m->delay_count++;
}

/* Helper: pre-fill EEPROM contents */
static void at24cxx_model_fill(at24cxx_hw_model_t *m, uint8_t addr,
                                const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len && ((uint16_t)addr + i) < AT24CXX_MODEL_SIZE; i++) {
        m->hw.regs[(uint8_t)((uint16_t)addr + i)] = data[i];
    }
}

#endif /* HW_MODEL_AT24CXX_H */
