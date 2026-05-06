/*
 * hw_model_max17048.h — MAX17048 hardware model from datasheet
 *
 * Models Maxim DS datasheet behavior:
 * - 16-bit register-based I2C interface at 0x36
 * - VCELL (0x02): cell voltage, 78.125 uV per LSB
 * - SOC (0x04): state-of-charge, 1/256 % per LSB
 * - MODE (0x06): quickstart bit 14
 * - VERSION (0x08): IC version (POR 0x0011)
 * - CONFIG (0x0C): sleep/alert/threshold (POR 0x971C)
 * - CRATE (0x16): charge rate, 0.208 %/hr per LSB (signed)
 * - STATUS (0x1A): status flags
 *
 * Uses the standard register-based mock_i2c callbacks.
 * Helpers convert physical units to raw register values.
 */

#ifndef HW_MODEL_MAX17048_H
#define HW_MODEL_MAX17048_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../max17048.h"

/* Register addresses (match driver internal defines) */
#define HW_REG_VCELL    0x02u
#define HW_REG_SOC      0x04u
#define HW_REG_MODE     0x06u
#define HW_REG_VERSION  0x08u
#define HW_REG_CONFIG   0x0Cu
#define HW_REG_CRATE    0x16u
#define HW_REG_STATUS   0x1Au

typedef struct {
    uint8_t  regs[256];
    int      write_count;
    int      read_count;
    bool     quickstart_triggered;   /* set when MODE bit 14 written */
} max17048_model_t;

/* POR defaults from datasheet */
static void max17048_model_init(max17048_model_t *m)
{
    memset(m, 0, sizeof(*m));

    /* VERSION = 0x0011 */
    m->regs[HW_REG_VERSION]     = 0x00u;
    m->regs[HW_REG_VERSION + 1] = 0x11u;

    /* CONFIG = 0x971C (RCOMP=0x97, sleep=0, alert=0, threshold=28 -> 4%) */
    m->regs[HW_REG_CONFIG]      = 0x97u;
    m->regs[HW_REG_CONFIG + 1]  = 0x1Cu;

    /* SOC = 0x6400 -> 100.0% */
    m->regs[HW_REG_SOC]         = 0x64u;
    m->regs[HW_REG_SOC + 1]     = 0x00u;

    /* VCELL = 0xCA80 -> ~4.05V (typical full LiPo) */
    m->regs[HW_REG_VCELL]       = 0xCAu;
    m->regs[HW_REG_VCELL + 1]   = 0x80u;

    /* CRATE = 0x0000 -> 0 %/hr (idle) */
    m->regs[HW_REG_CRATE]       = 0x00u;
    m->regs[HW_REG_CRATE + 1]   = 0x00u;

    /* MODE = 0x0000 */
    m->regs[HW_REG_MODE]        = 0x00u;
    m->regs[HW_REG_MODE + 1]    = 0x00u;

    /* STATUS = 0x0000 */
    m->regs[HW_REG_STATUS]      = 0x00u;
    m->regs[HW_REG_STATUS + 1]  = 0x00u;
}

/* ─── helpers: set physical values ──────────────────────────── */

/** Set cell voltage. Converts volts to raw VCELL (78.125 uV/LSB). */
static void max17048_model_set_voltage(max17048_model_t *m, float volts)
{
    uint16_t raw = (uint16_t)(volts / 78.125e-6f);
    m->regs[HW_REG_VCELL]     = (uint8_t)(raw >> 8);
    m->regs[HW_REG_VCELL + 1] = (uint8_t)(raw & 0xFFu);
}

/** Set SOC. Converts percent (0-100) to raw SOC (1/256 %/LSB). */
static void max17048_model_set_soc(max17048_model_t *m, float pct)
{
    uint16_t raw = (uint16_t)(pct * 256.0f);
    m->regs[HW_REG_SOC]       = (uint8_t)(raw >> 8);
    m->regs[HW_REG_SOC + 1]   = (uint8_t)(raw & 0xFFu);
}

/** Set charge rate. Converts %/hr to raw CRATE (0.208 %/hr/LSB, signed). */
static void max17048_model_set_crate(max17048_model_t *m, float pct_per_hr)
{
    int16_t raw = (int16_t)(pct_per_hr / 0.208f);
    m->regs[HW_REG_CRATE]     = (uint8_t)((uint16_t)raw >> 8);
    m->regs[HW_REG_CRATE + 1] = (uint8_t)((uint16_t)raw & 0xFFu);
}

/** Set VERSION register. */
static void max17048_model_set_version(max17048_model_t *m, uint16_t ver)
{
    m->regs[HW_REG_VERSION]     = (uint8_t)(ver >> 8);
    m->regs[HW_REG_VERSION + 1] = (uint8_t)(ver & 0xFFu);
}

/** Get alert threshold from CONFIG (bits 4:0 of low byte). Returns 32 - raw. */
static uint8_t max17048_model_get_alert_threshold(const max17048_model_t *m)
{
    return (uint8_t)(32u - (m->regs[HW_REG_CONFIG + 1] & 0x1Fu));
}

/** Check if sleep bit is set (bit 7 of CONFIG low byte). */
static bool max17048_model_is_sleeping(const max17048_model_t *m)
{
    return (m->regs[HW_REG_CONFIG + 1] & 0x80u) != 0;
}

/** Check if alert flag is set (bit 5 of CONFIG low byte). */
static bool max17048_model_is_alert_set(const max17048_model_t *m)
{
    return (m->regs[HW_REG_CONFIG + 1] & 0x20u) != 0;
}

/** Set alert flag in CONFIG. */
static void max17048_model_trigger_alert(max17048_model_t *m)
{
    m->regs[HW_REG_CONFIG + 1] |= 0x20u;
}

/* ─── I2C callbacks backed by model ─────────────────────────── */

__attribute__((unused))
static bool max17048_model_write_reg(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    max17048_model_t *m = (max17048_model_t *)ctx;
    m->write_count++;
    m->regs[reg] = val;

    /* Detect quickstart: MODE register bit 14 = byte[0] bit 6 */
    if (reg == HW_REG_MODE && (val & 0x40u)) {
        m->quickstart_triggered = true;
    }

    return true;
}

__attribute__((unused))
static bool max17048_model_read_regs(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    max17048_model_t *m = (max17048_model_t *)ctx;
    m->read_count++;

    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        buf[i] = (idx < 256) ? m->regs[idx] : 0xFFu;
    }
    return true;
}

#endif /* HW_MODEL_MAX17048_H */
