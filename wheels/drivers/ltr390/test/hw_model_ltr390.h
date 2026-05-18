// SPDX-License-Identifier: MIT
/*
 * hw_model_ltr390.h -- LTR-390UV hardware register model from datasheet
 *
 * Models LTR-390UV-01 register behavior:
 *   0x00  MAIN_CTRL    (R/W) — mode select [3], enable [1]
 *   0x04  MEAS_RATE    (R/W) — resolution [6:4], rate [2:0]
 *   0x05  GAIN         (R/W) — gain [2:0]
 *   0x06  PART_ID      (R)   — 0xB2 (part=0xB, rev=0x2)
 *   0x0D-0x0F  ALS_DATA (R)  — 20-bit little-endian
 *   0x10-0x12  UVS_DATA (R)  — 20-bit little-endian
 *
 * POR defaults per datasheet:
 *   MAIN_CTRL = 0x00 (standby, ALS mode)
 *   MEAS_RATE = 0x22 (18-bit, 100ms rate)
 *   GAIN      = 0x01 (3x)
 *   PART_ID   = 0xB2
 *   ALS/UVS   = 0x000000
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
} ltr390_model_t;

static ltr390_model_t ltr390_mdl;

static void hw_model_reset(void)
{
    memset(&ltr390_mdl, 0, sizeof(ltr390_mdl));

    /* POR defaults from LTR-390UV-01 datasheet */
    ltr390_mdl.regs[0x00] = 0x00;   /* MAIN_CTRL: standby, ALS mode */
    ltr390_mdl.regs[0x04] = 0x22;   /* MEAS_RATE: 18-bit, 100ms */
    ltr390_mdl.regs[0x05] = 0x01;   /* GAIN: 3x */
    ltr390_mdl.regs[0x06] = 0xB2;   /* PART_ID: part=0xB, rev=0x2 */

    /* ALS and UVS data default to 0 */
}

/** Inject a 20-bit ALS raw value into the model registers (0x0D-0x0F). */
static void hw_model_inject_als(uint32_t raw)
{
    raw &= 0x000FFFFFu;
    ltr390_mdl.regs[0x0D] = (uint8_t)(raw & 0xFFu);
    ltr390_mdl.regs[0x0E] = (uint8_t)((raw >> 8) & 0xFFu);
    ltr390_mdl.regs[0x0F] = (uint8_t)((raw >> 16) & 0x0Fu);
}

/** Inject a 20-bit UVS raw value into the model registers (0x10-0x12). */
static void hw_model_inject_uvs(uint32_t raw)
{
    raw &= 0x000FFFFFu;
    ltr390_mdl.regs[0x10] = (uint8_t)(raw & 0xFFu);
    ltr390_mdl.regs[0x11] = (uint8_t)((raw >> 8) & 0xFFu);
    ltr390_mdl.regs[0x12] = (uint8_t)((raw >> 16) & 0x0Fu);
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
        buf[i] = ltr390_mdl.regs[(uint8_t)(reg + i)];
    }
    return true;
}

__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

#endif /* HW_MODEL_LTR390_H */
