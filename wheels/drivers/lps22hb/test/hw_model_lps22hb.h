// SPDX-License-Identifier: MIT
/*
 * hw_model_lps22hb.h — LPS22HB hardware model from datasheet
 *
 * Models ST LPS22HB (DocID027083) behavior:
 * - Register-based I2C interface at 0x5C (SA0=0) or 0x5D (SA0=1)
 * - 24-bit pressure output, 16-bit temperature output
 * - WHO_AM_I (0x0F) = 0xB1
 * - CTRL_REG1 (0x10): ODR[6:4], BDU (bit 1)
 * - CTRL_REG2 (0x11): ONE_SHOT (bit 0), auto-clears after conversion
 * - STATUS (0x27): P_DA (bit 0), T_DA (bit 1)
 * - BDU enforcement: when BDU=1, output registers hold their sample until
 *   PRESS_OUT_H (0x2A) is read
 * - IF_ADD_INC (CTRL_REG2 bit 4): a multi-byte read stays on one register
 *   when clear; bursts on output registers under BDU are counted
 * - One-shot: setting ONE_SHOT bit triggers conversion, then self-clears
 *
 * Uses the standard register-based I2C callbacks.
 */

#ifndef HW_MODEL_LPS22HB_H
#define HW_MODEL_LPS22HB_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Register addresses (match lps22hb.c) */
#define LPS22HB_MODEL_REG_WHO_AM_I    0x0Fu
#define LPS22HB_MODEL_REG_CTRL_REG1   0x10u
#define LPS22HB_MODEL_REG_CTRL_REG2   0x11u
#define LPS22HB_MODEL_REG_STATUS      0x27u
#define LPS22HB_MODEL_REG_PRESS_XL    0x28u
#define LPS22HB_MODEL_REG_PRESS_L     0x29u
#define LPS22HB_MODEL_REG_PRESS_H     0x2Au
#define LPS22HB_MODEL_REG_TEMP_L      0x2Bu
#define LPS22HB_MODEL_REG_TEMP_H      0x2Cu

/* POR defaults per datasheet Table 16 */
#define LPS22HB_MODEL_POR_WHO_AM_I    0xB1u
#define LPS22HB_MODEL_POR_CTRL_REG1   0x00u
#define LPS22HB_MODEL_POR_CTRL_REG2   0x10u  /* BOOT=0, IF_ADD_INC=1 */
#define LPS22HB_MODEL_POR_STATUS      0x00u

#define LPS22HB_MODEL_REG_COUNT       256

typedef struct {
    uint8_t regs[LPS22HB_MODEL_REG_COUNT];

    /* Shadow pressure/temperature raw values (injected by test) */
    int32_t pressure_raw;      /* 24-bit two's complement */
    int16_t temperature_raw;   /* 16-bit two's complement */

    /* Data-available flags set by inject, cleared by reading output regs */
    bool    p_da;
    bool    t_da;

    /* One-shot state: becomes true when ONE_SHOT written, cleared on next
     * read of output registers (simulates conversion complete) */
    bool    one_shot_pending;

    /* BDU: the output registers hold a sample until PRESS_OUT_H is read, and
     * a newer one waits in the shadow values */
    bool    held;
    bool    sample_waiting;
    int     bdu_bursts;   /* multi-byte reads of output registers under BDU */

    /* Counters for observability */
    int     write_count;
    int     read_count;
} lps22hb_model_t;

static void lps22hb_model_init(lps22hb_model_t *m)
{
    memset(m, 0, sizeof(*m));
    m->regs[LPS22HB_MODEL_REG_WHO_AM_I]  = LPS22HB_MODEL_POR_WHO_AM_I;
    m->regs[LPS22HB_MODEL_REG_CTRL_REG1] = LPS22HB_MODEL_POR_CTRL_REG1;
    m->regs[LPS22HB_MODEL_REG_CTRL_REG2] = LPS22HB_MODEL_POR_CTRL_REG2;
    m->regs[LPS22HB_MODEL_REG_STATUS]    = LPS22HB_MODEL_POR_STATUS;
}

/* Copy the shadow values into the output registers */
static void lps22hb_model_refresh(lps22hb_model_t *m)
{
    uint32_t up = (uint32_t)m->pressure_raw;
    uint16_t ut = (uint16_t)m->temperature_raw;
    m->regs[LPS22HB_MODEL_REG_PRESS_XL] = (uint8_t)(up & 0xFFu);
    m->regs[LPS22HB_MODEL_REG_PRESS_L]  = (uint8_t)((up >> 8) & 0xFFu);
    m->regs[LPS22HB_MODEL_REG_PRESS_H]  = (uint8_t)((up >> 16) & 0xFFu);
    m->regs[LPS22HB_MODEL_REG_TEMP_L]   = (uint8_t)(ut & 0xFFu);
    m->regs[LPS22HB_MODEL_REG_TEMP_H]   = (uint8_t)((ut >> 8) & 0xFFu);
}

/* A new sample reaches the registers unless BDU is holding the last one */
static void lps22hb_model_new_sample(lps22hb_model_t *m)
{
    bool bdu = (m->regs[LPS22HB_MODEL_REG_CTRL_REG1] & 0x02u) != 0;
    if (bdu && m->held) {
        m->sample_waiting = true;
        return;
    }
    lps22hb_model_refresh(m);
    m->held = bdu;
}

/* Inject pressure raw value and mark data available */
static void lps22hb_model_inject_pressure(lps22hb_model_t *m, int32_t raw)
{
    m->pressure_raw = raw;
    m->p_da = true;
    m->regs[LPS22HB_MODEL_REG_STATUS] |= 0x01u;  /* P_DA */
    lps22hb_model_new_sample(m);
}

/* Inject temperature raw value and mark data available */
static void lps22hb_model_inject_temperature(lps22hb_model_t *m, int16_t raw)
{
    m->temperature_raw = raw;
    m->t_da = true;
    m->regs[LPS22HB_MODEL_REG_STATUS] |= 0x02u;  /* T_DA */
    lps22hb_model_new_sample(m);
}

/* Inject both pressure and temperature at once */
static void lps22hb_model_inject(lps22hb_model_t *m,
                                 int32_t pressure_raw,
                                 int16_t temperature_raw)
{
    m->pressure_raw = pressure_raw;
    m->temperature_raw = temperature_raw;
    m->p_da = true;
    m->t_da = true;
    m->regs[LPS22HB_MODEL_REG_STATUS] |= 0x03u;
    lps22hb_model_new_sample(m);
}

/* I2C write callback: models register write behavior */
__attribute__((unused))
static bool lps22hb_model_write_reg(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    lps22hb_model_t *m = (lps22hb_model_t *)ctx;
    m->write_count++;

    switch (reg) {
    case LPS22HB_MODEL_REG_WHO_AM_I:
        /* Read-only register, write is silently ignored per datasheet */
        break;

    case LPS22HB_MODEL_REG_CTRL_REG1:
        m->regs[reg] = val;
        break;

    case LPS22HB_MODEL_REG_CTRL_REG2:
        m->regs[reg] = val;
        /* ONE_SHOT bit (bit 0): triggers single conversion when ODR=0 */
        if (val & 0x01u) {
            uint8_t odr = (m->regs[LPS22HB_MODEL_REG_CTRL_REG1] >> 4) & 0x07u;
            if (odr == 0) {
                /* In one-shot mode: simulate immediate conversion —
                 * refresh output registers from shadow and set DA flags.
                 * The ONE_SHOT bit self-clears after conversion. */
                m->one_shot_pending = true;
                lps22hb_model_new_sample(m);
                m->p_da = true;
                m->t_da = true;
                m->regs[LPS22HB_MODEL_REG_STATUS] |= 0x03u;
                /* Self-clear ONE_SHOT bit */
                m->regs[LPS22HB_MODEL_REG_CTRL_REG2] &= (uint8_t)~0x01u;
                m->one_shot_pending = false;
            }
        }
        break;

    case LPS22HB_MODEL_REG_STATUS:
        /* Read-only, ignore writes */
        break;

    case LPS22HB_MODEL_REG_PRESS_XL:
    case LPS22HB_MODEL_REG_PRESS_L:
    case LPS22HB_MODEL_REG_PRESS_H:
    case LPS22HB_MODEL_REG_TEMP_L:
    case LPS22HB_MODEL_REG_TEMP_H:
        /* Output registers are read-only, ignore writes */
        break;

    default:
        m->regs[reg] = val;
        break;
    }

    return true;
}

/* I2C read callback: models register read behavior with BDU */
__attribute__((unused))
static bool lps22hb_model_read_regs(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    lps22hb_model_t *m = (lps22hb_model_t *)ctx;
    m->read_count++;

    bool bdu = (m->regs[LPS22HB_MODEL_REG_CTRL_REG1] & 0x02u) != 0;
    bool inc = (m->regs[LPS22HB_MODEL_REG_CTRL_REG2] & 0x10u) != 0;

    if (bdu && len > 1 && reg >= LPS22HB_MODEL_REG_PRESS_XL &&
        reg <= LPS22HB_MODEL_REG_TEMP_H) {
        m->bdu_bursts++;
    }

    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + (inc ? i : 0u);
        buf[i] = (idx < LPS22HB_MODEL_REG_COUNT) ? m->regs[idx] : 0x00u;

        /* Reading an MSB clears its flag, and PRESS_OUT_H ends the BDU hold */
        if (idx == LPS22HB_MODEL_REG_TEMP_H) {
            m->t_da = false;
            m->regs[LPS22HB_MODEL_REG_STATUS] &= (uint8_t)~0x02u;
        }
        if (idx == LPS22HB_MODEL_REG_PRESS_H) {
            m->p_da = false;
            m->regs[LPS22HB_MODEL_REG_STATUS] &= (uint8_t)~0x01u;
            m->held = false;
            if (bdu && m->sample_waiting) {
                m->sample_waiting = false;
                lps22hb_model_new_sample(m);
            }
        }
    }

    return true;
}

#endif /* HW_MODEL_LPS22HB_H */
