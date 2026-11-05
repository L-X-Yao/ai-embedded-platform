// SPDX-License-Identifier: MIT
/*
 * hw_model_cc1101.h — generic SPI scaffold for cc1101, unused
 *
 * No test includes this file. test_cc1101.c has its own mock of the header
 * byte, strobes and FIFOs, and the driver is tested against that mock.
 * Nothing here was taken from the datasheet.
 */
#ifndef HW_MODEL_CC1101_H
#define HW_MODEL_CC1101_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_xfer_count;
static int     hw_cs_state;
static uint8_t hw_addr_latch;
static int     hw_byte_idx;

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_xfer_count = 0;
    hw_cs_state   = 1;
    hw_addr_latch = 0;
    hw_byte_idx   = 0;
}

__attribute__((unused))
static void hw_model_cs_low(void *ctx) { (void)ctx; hw_cs_state = 0; hw_byte_idx = 0; }

__attribute__((unused))
static void hw_model_cs_high(void *ctx) { (void)ctx; hw_cs_state = 1; }

__attribute__((unused))
static uint8_t hw_model_xfer(void *ctx, uint8_t tx)
{
    (void)ctx;
    hw_xfer_count++;
    if (hw_byte_idx == 0) {
        hw_addr_latch = tx;
        hw_byte_idx++;
        return 0xFF;
    }
    uint8_t reg = (uint8_t)((hw_addr_latch & 0x7F) + hw_byte_idx - 1);
    uint8_t ret = hw_regs[reg];
    if (!(hw_addr_latch & 0x80)) {
        hw_regs[reg] = tx;
    }
    hw_byte_idx++;
    return ret;
}

#endif /* HW_MODEL_CC1101_H */
