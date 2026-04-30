/*
 * test_mock_i2c.h — shared I2C register mock for L1 driver tests
 *
 * Header-only. Include from any test_*.c that tests a register-based
 * I2C driver. Provides mock_regs[], fail injection, and standard
 * write_reg/read_regs callbacks matching the i2c_hal template.
 *
 * Usage:
 *   #include "test_harness.h"
 *   #include "test_mock_i2c.h"
 *   #include "my_driver.h"
 *
 *   static void mock_reset(void) {
 *       mock_i2c_reset();          // clears regs + fail state
 *       mock_regs[0xD0] = 0x58;   // chip-specific WHO_AM_I
 *   }
 *
 *   static const my_driver_i2c_t mock_i2c = {
 *       .write_reg = mock_i2c_write_reg,
 *       .read_regs = mock_i2c_read_regs,
 *       .ctx       = NULL,
 *   };
 */

#ifndef TEST_MOCK_I2C_H
#define TEST_MOCK_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef MOCK_I2C_REG_COUNT
#define MOCK_I2C_REG_COUNT 256
#endif

static uint8_t mock_regs[MOCK_I2C_REG_COUNT];
static int     mock_fail_next;
static int     mock_op_count;
static int     mock_fail_at;
static int     mock_write_count;
static int     mock_read_count;

static void mock_i2c_reset(void)
{
    memset(mock_regs, 0, sizeof(mock_regs));
    mock_fail_next  = 0;
    mock_op_count   = 0;
    mock_fail_at    = -1;
    mock_write_count = 0;
    mock_read_count  = 0;
}

static bool mock_i2c_should_fail(void)
{
    int n = mock_op_count++;
    if (mock_fail_next) { mock_fail_next = 0; return true; }
    if (mock_fail_at >= 0 && n == mock_fail_at) return true;
    return false;
}

__attribute__((unused))
static bool mock_i2c_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_write_count++;
    mock_regs[reg] = val;
    return true;
}

__attribute__((unused))
static bool mock_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (mock_i2c_should_fail()) return false;
    mock_read_count++;
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < MOCK_I2C_REG_COUNT) ? mock_regs[idx] : 0xFF;
    }
    return true;
}

#endif /* TEST_MOCK_I2C_H */
