// SPDX-License-Identifier: MIT
/*
 * hw_model_ltr390.h -- LTR-390UV hardware register model from datasheet
 *
 * Models LTR-390UV-01 register behavior:
 *   0x00  MAIN_CTRL    (R/W) — mode select [3], enable [1]
 *   0x04  MEAS_RATE    (R/W) — resolution [6:4], rate [2:0]
 *   0x05  GAIN         (R/W) — gain [2:0]
 *   0x06  PART_ID      (R)   — 0xB2 (part=0xB, rev=0x2)
 *   0x07  MAIN_STATUS  (R)   — power-on [5], interrupt [4], new data [3]
 *   0x0D-0x0F  ALS_DATA (R)  — 20-bit little-endian
 *   0x10-0x12  UVS_DATA (R)  — 20-bit little-endian
 *
 * POR defaults per datasheet p.12:
 *   MAIN_CTRL   = 0x00 (standby, ALS mode)
 *   MEAS_RATE   = 0x22 (18-bit, 100ms rate)
 *   GAIN        = 0x01 (3x)
 *   PART_ID     = 0xB2
 *   MAIN_STATUS = 0x20
 *   ALS/UVS     = 0x000000
 *   INT_CFG     = 0x10
 *   THRES_UP    = 0xFF 0xFF 0x0F
 *
 * Time passes through hw_model_delay_ms, or hw_model_advance_us in a test. In
 * active mode the channel of the mode converts after the wake-up time, and the
 * data registers and the new-data bit change only when a conversion ends. So
 * hw_model_inject_als and hw_model_inject_uvs set what the next conversion gives,
 * and a read before it gets the old data, as on the chip.
 */
#ifndef HW_MODEL_LTR390_H
#define HW_MODEL_LTR390_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

typedef struct {
    uint8_t regs[HW_REG_COUNT];
    int     write_count;
    int     read_count;
    bool    fail_next_read;
    bool    fail_next_write;
    uint32_t now_us;
    uint32_t next_end_us;   /* the end of the conversion under way */
    bool     active;
    uint32_t als_raw;       /* what the next ALS conversion gives */
    uint32_t uvs_raw;       /* what the next UVS conversion gives */
} ltr390_model_t;

/* p.5 and p.25: wake-up from standby, typically 5 ms */
#define HW_WAKEUP_US 5000u

static ltr390_model_t ltr390_mdl;

/* The p.12 table, which is also what a software reset comes back to */
static void hw_model_defaults(void)
{
    memset(ltr390_mdl.regs, 0, sizeof(ltr390_mdl.regs));
    ltr390_mdl.active = false;

    ltr390_mdl.regs[0x00] = 0x00;   /* MAIN_CTRL: standby, ALS mode */
    ltr390_mdl.regs[0x04] = 0x22;   /* MEAS_RATE: 18-bit, 100ms */
    ltr390_mdl.regs[0x05] = 0x01;   /* GAIN: 3x */
    ltr390_mdl.regs[0x06] = 0xB2;   /* PART_ID: part=0xB, rev=0x2 */
    ltr390_mdl.regs[0x07] = 0x20;   /* MAIN_STATUS: power-on event */
    ltr390_mdl.regs[0x19] = 0x10;   /* INT_CFG */
    ltr390_mdl.regs[0x21] = 0xFF;   /* THRES_UP_0 */
    ltr390_mdl.regs[0x22] = 0xFF;   /* THRES_UP_1 */
    ltr390_mdl.regs[0x23] = 0x0F;   /* THRES_UP_2 */

    /* ALS and UVS data default to 0 */
}

static void hw_model_reset(void)
{
    memset(&ltr390_mdl, 0, sizeof(ltr390_mdl));
    hw_model_defaults();
}

/* p.14: conversion time by resolution, 110 and 111 are reserved */
static uint32_t hw_model_conversion_us(void)
{
    static const uint32_t us[8] = {
        400000u, 200000u, 100000u, 50000u, 25000u, 12500u, 100000u, 100000u,
    };
    return us[(ltr390_mdl.regs[0x04] >> 4) & 0x07u];
}

/* p.14: the measurement rate, "lowered" when the conversion cannot keep up */
static uint32_t hw_model_period_us(void)
{
    static const uint32_t us[8] = {
        25000u, 50000u, 100000u, 200000u, 500000u, 1000000u, 2000000u, 2000000u,
    };
    uint32_t rate = us[ltr390_mdl.regs[0x04] & 0x07u];
    uint32_t conversion = hw_model_conversion_us();
    return rate > conversion ? rate : conversion;
}

static void hw_model_store(uint8_t reg, uint32_t raw)
{
    ltr390_mdl.regs[reg]                = (uint8_t)(raw & 0xFFu);
    ltr390_mdl.regs[(uint8_t)(reg + 1)] = (uint8_t)((raw >> 8) & 0xFFu);
    ltr390_mdl.regs[(uint8_t)(reg + 2)] = (uint8_t)((raw >> 16) & 0x0Fu);
}

/* p.13: "UVS Mode: UVS+UVS Comp channels activated, ALS and Comp channels
 * deactivated" */
static void hw_model_conversion_ends(void)
{
    if (ltr390_mdl.regs[0x00] & 0x08u) {
        hw_model_store(0x10, ltr390_mdl.uvs_raw);
    } else {
        hw_model_store(0x0D, ltr390_mdl.als_raw);
    }
    ltr390_mdl.regs[0x07] |= 0x08u;   /* p.16: new data */
}

__attribute__((unused))
static void hw_model_advance_us(uint32_t us)
{
    uint32_t end = ltr390_mdl.now_us + us;

    while (ltr390_mdl.active && ltr390_mdl.next_end_us <= end) {
        hw_model_conversion_ends();
        ltr390_mdl.next_end_us += hw_model_period_us();
    }
    ltr390_mdl.now_us = end;
}

/* Let the conversion under way end */
__attribute__((unused))
static void hw_model_finish_conversion(void)
{
    if (ltr390_mdl.active) {
        hw_model_advance_us(ltr390_mdl.next_end_us - ltr390_mdl.now_us);
    }
}

/* p.13: "When writing to this register, it will cause a stop to any ongoing
 * measurements ALS/UVS and start new measurement." */
static void hw_model_write_main_ctrl(uint8_t val)
{
    bool was_active = ltr390_mdl.active;

    if (val & 0x10u) {
        hw_model_defaults();   /* bit 4: software reset */
        return;
    }
    ltr390_mdl.regs[0x00] = val;
    ltr390_mdl.active = (val & 0x02u) != 0;
    ltr390_mdl.next_end_us = ltr390_mdl.now_us + hw_model_conversion_us() +
                             (was_active ? 0u : HW_WAKEUP_US);
}

/** Set the 20-bit ALS value the next conversion gives. */
static void hw_model_inject_als(uint32_t raw)
{
    ltr390_mdl.als_raw = raw & 0x000FFFFFu;
}

/** Set the 20-bit UVS value the next conversion gives. */
static void hw_model_inject_uvs(uint32_t raw)
{
    ltr390_mdl.uvs_raw = raw & 0x000FFFFFu;
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    ltr390_mdl.write_count++;

    if (ltr390_mdl.fail_next_write) {
        ltr390_mdl.fail_next_write = false;
        return false;
    }

    /* Enforce read-only registers */
    switch (reg) {
    case 0x06: /* PART_ID — read-only, ignore writes */
    case 0x07: /* MAIN_STATUS — read-only */
    case 0x0D: case 0x0E: case 0x0F: /* ALS data — read-only */
    case 0x10: case 0x11: case 0x12: /* UVS data — read-only */
    case 0x13: case 0x14: case 0x15: /* UVS_COMP data — read-only */
    case 0x16: case 0x17: case 0x18: /* COMP data — read-only */
        break;
    case 0x00:
        hw_model_write_main_ctrl(val);
        break;
    default:
        ltr390_mdl.regs[reg] = val;
        break;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    ltr390_mdl.read_count++;

    if (ltr390_mdl.fail_next_read) {
        ltr390_mdl.fail_next_read = false;
        return false;
    }

    for (uint16_t i = 0; i < len; i++) {
        uint8_t addr = (uint8_t)(reg + i);
        buf[i] = ltr390_mdl.regs[addr];
        /* p.16: power-on, interrupt and new-data status clear after read */
        if (addr == 0x07) {
            ltr390_mdl.regs[0x07] &= (uint8_t)~0x38u;
        }
        /* p.16: "UVS/ALS data is old data (Data has been read)" */
        if (addr >= 0x0D && addr <= 0x12) {
            ltr390_mdl.regs[0x07] &= (uint8_t)~0x08u;
        }
    }
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    hw_model_advance_us(ms * 1000u);
}

#endif /* HW_MODEL_LTR390_H */
