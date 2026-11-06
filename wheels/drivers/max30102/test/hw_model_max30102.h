// SPDX-License-Identifier: MIT
/*
 * hw_model_max30102.h -- MAX30102 register-based hardware model
 *
 * Models Maxim MAX30102 (datasheet 19-7740 Rev 0) register behavior. Page
 * numbers are those of that PDF.
 * - Part ID 0x15 at register 0xFF (read-only), answers at I2C address 0x57
 * - Rev ID at register 0xFE (read-only, default 0x01)
 * - Software reset via MODE_CONFIG (0x09) bit 6: everything back to POR state
 * - FIFO with 32 slots, 3 bytes per channel (18-bit ADC, left-justified)
 * - FIFO_WR_PTR (0x04): writable only in modes 010, 011 and 111
 * - OVF_COUNTER (0x05): samples lost, saturates at 0xF, clears on a pop
 * - FIFO_RD_PTR (0x06): moves on the first byte of a sample, writable to reread
 * - FIFO_DATA (0x07): burst read walks the samples
 * - Die temperature: TEMP_INT (0x1F, signed), TEMP_FRAC (0x20, 4-bit)
 * - TEMP_CONFIG (0x21): a conversion takes 29 ms of delay callback time, then
 *   TEMP_EN clears and DIE_TEMP_RDY sets
 * - Interrupt status registers clear on read
 * - LED1_PA (0x0C), LED2_PA (0x0D): about 0.2 mA per step, typical (Table 8)
 * - MODE_CONFIG (0x09): mode[2:0], SHDN bit 7, RESET bit 6
 * - SPO2_CONFIG (0x0A): ADC_RANGE[6:5], SR[4:2], PW[1:0], with a rate above
 *   Tables 11 and 12 replaced by the highest one allowed
 * - FIFO_CONFIG (0x08): SMP_AVE[7:5], FIFO_ROLLOVER[4], FIFO_A_FULL[3:0]
 *
 * Not modelled: sample flow in time (tests push samples), the A_FULL, PPG_RDY
 * and ALC_OVF flags, proximity mode and Multi-LED slots.
 */

#ifndef HW_MODEL_MAX30102_H
#define HW_MODEL_MAX30102_H

#include "hw_model.h"

/* --- I2C address --- */
#define MAX_I2C_ADDR          0x57u

/* --- MAX30102 register addresses (datasheet "Register Maps and Descriptions") --- */
#define MAX_REG_INT_STATUS_1   0x00u
#define MAX_REG_INT_STATUS_2   0x01u
#define MAX_REG_INT_ENABLE_1   0x02u
#define MAX_REG_INT_ENABLE_2   0x03u
#define MAX_REG_FIFO_WR_PTR   0x04u
#define MAX_REG_OVF_COUNTER   0x05u
#define MAX_REG_FIFO_RD_PTR   0x06u
#define MAX_REG_FIFO_DATA     0x07u
#define MAX_REG_FIFO_CONFIG   0x08u
#define MAX_REG_MODE_CONFIG   0x09u
#define MAX_REG_SPO2_CONFIG   0x0Au
#define MAX_REG_LED1_PA       0x0Cu
#define MAX_REG_LED2_PA       0x0Du
#define MAX_REG_RESERVED_13   0x13u
#define MAX_REG_RESERVED_17   0x17u
#define MAX_REG_TEMP_INT      0x1Fu
#define MAX_REG_TEMP_FRAC     0x20u
#define MAX_REG_TEMP_CONFIG   0x21u
#define MAX_REG_REV_ID        0xFEu
#define MAX_REG_PART_ID       0xFFu

/* --- Register bits --- */
#define MAX_INT1_PWR_RDY      0x01u
#define MAX_INT2_DIE_TEMP_RDY 0x02u
#define MAX_FIFO_ROLLOVER_EN  0x10u
#define MAX_MODE_MASK         0x07u
#define MAX_MODE_HR           0x02u
#define MAX_MODE_SPO2         0x03u
#define MAX_MODE_MULTI_LED    0x07u
#define MAX_MODE_RESET        0x40u
#define MAX_MODE_SHDN         0x80u
#define MAX_TEMP_EN           0x01u

/* --- FIFO depth --- */
#define MAX_FIFO_DEPTH        32u
/* p.14: OVF_COUNTER "saturates at 0xF" */
#define MAX_OVF_MAX           0x0Fu

/* --- p.5: Temperature ADC Acquisition Time --- */
#define MAX_TEMP_ACQ_MS       29u

/* --- Model structure --- */
typedef struct {
    hw_reg_model_t  hw;
    /* FIFO storage: up to 32 samples, each has RED (3 bytes) + IR (3 bytes) */
    uint8_t         fifo[MAX_FIFO_DEPTH * 6];
    uint8_t         fifo_wr;        /* 5-bit write pointer   */
    uint8_t         fifo_rd;        /* 5-bit read pointer    */
    uint8_t         fifo_unread;    /* 0..32: equal pointers are 0 or 32 */
    uint8_t         fifo_mode;      /* MODE[2:0], 0 at POR   */
    /* FIFO_DATA reads with no sample there: a driver has no reason to do it */
    int             fifo_empty_reads;
    /* Simulated die temperature */
    float           true_temp_c;
    /* Time left in a temperature conversion, 0 = none running */
    uint32_t        temp_ms_left;
    /* Counters */
    int             reset_count;
    int             temp_trigger_count;
} max30102_model_t;

/* --- Write callback: MODE_CONFIG handles reset --- */
__attribute__((unused))
static void max30102_on_mode_config(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    max30102_model_t *m = (max30102_model_t *)user;

    /* p.19: "all configuration, threshold, and data registers are reset to
     * their power-on-state through a power-on reset. The RESET bit is cleared
     * automatically back to zero", and PWR_RDY stays out of it */
    if (new_val & MAX_MODE_RESET) {
        hw_model_reset(&m->hw);
        m->fifo_wr = 0;
        m->fifo_rd = 0;
        m->fifo_unread = 0;
        m->fifo_mode = 0;
        m->temp_ms_left = 0;
        m->reset_count++;
        return;
    }

    /* p.19: "Changing modes does not change any other setting, nor does it
     * erase any previously stored data" */
    m->fifo_mode = new_val & MAX_MODE_MASK;

    /* p.19: in shutdown "All interrupts are cleared to zero" */
    if (new_val & MAX_MODE_SHDN) {
        m->hw.regs[MAX_REG_INT_STATUS_1] = 0;
        m->hw.regs[MAX_REG_INT_STATUS_2] = 0;
    }
}

/* --- Write callback: the pointers, which the FIFO logic follows --- */
__attribute__((unused))
static void max30102_on_fifo_ptr(void *user, uint8_t reg,
                                 uint8_t old_val, uint8_t new_val)
{
    max30102_model_t *m = (max30102_model_t *)user;

    if (reg == MAX_REG_FIFO_WR_PTR) {
        /* p.14: writable "when MODE[2:0] is 010, 011, or 111" */
        if (m->fifo_mode != MAX_MODE_HR && m->fifo_mode != MAX_MODE_SPO2 &&
            m->fifo_mode != MAX_MODE_MULTI_LED) {
            m->hw.regs[MAX_REG_FIFO_WR_PTR] = old_val;
            return;
        }
        m->fifo_wr = new_val;
    } else {
        m->fifo_rd = new_val;
    }
    m->fifo_unread = (uint8_t)((m->fifo_wr - m->fifo_rd) & 0x1Fu);
}

/* --- Write callback: SPO2_CONFIG sample rate limit --- */

/* p.20: "If the user selects a sample rate that is too high for the selected
 * LED_PW setting, the highest possible sample rate is programmed instead into
 * the register." Tables 11 and 12 on p.24, per LED_PW 69, 118, 215 and 411 us:
 * SpO2 stops at 1600, 1000, 800 and 400 sps, HR at 3200, 1600, 1600 and 1000. */
__attribute__((unused))
static void max30102_on_spo2_config(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    static const uint8_t spo2_max[4] = {6u, 5u, 4u, 3u};
    static const uint8_t hr_max[4]   = {7u, 6u, 6u, 5u};
    (void)reg; (void)old_val;
    max30102_model_t *m = (max30102_model_t *)user;

    uint8_t pw  = (uint8_t)(new_val & 0x03u);
    uint8_t sr  = (uint8_t)((new_val >> 2) & 0x07u);
    uint8_t top = sr;

    if (m->fifo_mode == MAX_MODE_SPO2)
        top = spo2_max[pw];
    else if (m->fifo_mode == MAX_MODE_HR)
        top = hr_max[pw];

    if (sr > top) {
        m->hw.regs[MAX_REG_SPO2_CONFIG] =
            (uint8_t)((new_val & 0x63u) | (uint8_t)(top << 2));
    }
}

/* --- Write callback: TEMP_CONFIG starts a one-shot conversion --- */
__attribute__((unused))
static void max30102_on_temp_config(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    max30102_model_t *m = (max30102_model_t *)user;

    if ((new_val & MAX_TEMP_EN) && m->temp_ms_left == 0) {
        m->temp_ms_left = MAX_TEMP_ACQ_MS;
        m->temp_trigger_count++;
    }
}

/* The conversion ends: the result lands and TEMP_EN clears (p.23), and
 * DIE_TEMP_RDY sets (p.12) */
__attribute__((unused))
static void max30102_temp_done(max30102_model_t *m)
{
    /* Compute TEMP_INT (signed 8-bit) and TEMP_FRAC (4-bit, 0.0625 steps) */
    int8_t int_part = (int8_t)m->true_temp_c;
    float frac = m->true_temp_c - (float)int_part;
    if (frac < 0.0f) {
        int_part--;
        frac += 1.0f;
    }
    uint8_t frac_part = (uint8_t)(frac / 0.0625f) & 0x0Fu;

    m->hw.regs[MAX_REG_TEMP_INT] = (uint8_t)int_part;
    m->hw.regs[MAX_REG_TEMP_FRAC] = frac_part;

    m->hw.regs[MAX_REG_TEMP_CONFIG] = 0x00u;
    m->hw.regs[MAX_REG_INT_STATUS_2] |= MAX_INT2_DIE_TEMP_RDY;
}

/* --- I2C write: the device answers at its own address only --- */
__attribute__((unused))
static bool max30102_model_write_reg(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, uint8_t val)
{
    if (dev_addr != MAX_I2C_ADDR) return false;
    return hw_model_i2c_write_reg(ctx, dev_addr, reg, val);
}

/* --- Custom read callback: FIFO_DATA returns sample bytes --- */

/* Take the oldest sample off the FIFO and return its slot. p.16: "The FIFO read
 * pointer is automatically incremented after the first byte of each sample is
 * read", and p.14: when "the read pointer advances, OVF_COUNTER is reset to
 * zero". */
__attribute__((unused))
static uint8_t max30102_fifo_pop(max30102_model_t *m)
{
    uint8_t slot = m->fifo_rd;
    m->fifo_rd = (uint8_t)((m->fifo_rd + 1u) & 0x1Fu);
    m->hw.regs[MAX_REG_FIFO_RD_PTR] = m->fifo_rd;
    m->hw.regs[MAX_REG_OVF_COUNTER] = 0;
    m->fifo_unread--;
    return slot;
}

/* Override the default read_regs for what reading does on this chip. FIFO_DATA
 * does not move the register address (p.15), and each byte comes from the FIFO
 * storage. Status flags clear when read. */
__attribute__((unused))
static bool max30102_model_read_regs(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, uint8_t *buf, uint16_t len)
{
    max30102_model_t *m = (max30102_model_t *)ctx;

    if (dev_addr != MAX_I2C_ADDR) return false;

    if (reg == MAX_REG_FIFO_DATA) {
        if (hw_model__should_fail(&m->hw)) return false;

        /* 3 bytes per active channel, Table 4 */
        uint8_t bytes_per_sample = (m->fifo_mode == MAX_MODE_SPO2 ||
                                    m->fifo_mode == MAX_MODE_MULTI_LED) ? 6u : 3u;
        uint8_t slot = 0;
        bool    have = false;

        for (uint16_t i = 0; i < len; i++) {
            uint8_t at = (uint8_t)(i % bytes_per_sample);
            if (at == 0) {
                have = m->fifo_unread > 0;
                if (have)
                    slot = max30102_fifo_pop(m);
                else
                    m->fifo_empty_reads++;
            }
            buf[i] = have ? m->fifo[slot * 6u + at] : 0x00u;
        }
        return true;
    }

    /* Default register read */
    if (!hw_model_i2c_read_regs(ctx, dev_addr, reg, buf, len))
        return false;

    /* p.14: "The interrupts are cleared whenever the interrupt status register
     * is read", and p.12: DIE_TEMP_RDY also by reading "the TFRAC register" */
    for (uint16_t i = 0; i < len; i++) {
        unsigned idx = (unsigned)reg + i;
        if (idx == MAX_REG_INT_STATUS_1 || idx == MAX_REG_INT_STATUS_2)
            m->hw.regs[idx] = 0;
        else if (idx == MAX_REG_TEMP_FRAC)
            m->hw.regs[MAX_REG_INT_STATUS_2] &=
                (uint8_t)~MAX_INT2_DIE_TEMP_RDY;
    }
    return true;
}

/* --- Model initialization --- */

__attribute__((unused))
static void max30102_model_init(max30102_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Part ID = 0x15 (read-only) */
    hw_model_set_reset_value(&m->hw, MAX_REG_PART_ID, 0x15u);
    hw_model_set_readonly(&m->hw, MAX_REG_PART_ID);

    /* Rev ID = 0x01 (read-only) */
    hw_model_set_reset_value(&m->hw, MAX_REG_REV_ID, 0x01u);
    hw_model_set_readonly(&m->hw, MAX_REG_REV_ID);

    /* p.12: RESERVED 0x13-0x17 have POR state 0xFF */
    for (uint8_t r = MAX_REG_RESERVED_13; r <= MAX_REG_RESERVED_17; r++)
        hw_model_set_reset_value(&m->hw, r, 0xFFu);

    /* Interrupt status registers are read-clear (read-only from write) */
    hw_model_set_readonly(&m->hw, MAX_REG_INT_STATUS_1);
    hw_model_set_readonly(&m->hw, MAX_REG_INT_STATUS_2);

    /* The FIFO registers take writes (p.15), the pointers in 5 bits (p.13) */
    hw_model_set_writable_mask(&m->hw, MAX_REG_FIFO_WR_PTR, 0x1Fu);
    hw_model_set_writable_mask(&m->hw, MAX_REG_OVF_COUNTER, 0x1Fu);
    hw_model_set_writable_mask(&m->hw, MAX_REG_FIFO_RD_PTR, 0x1Fu);

    /* SPO2_CONFIG bit 7 is reserved 0 (p.11) */
    hw_model_set_writable_mask(&m->hw, MAX_REG_SPO2_CONFIG, 0x7Fu);

    /* TEMP_INT and TEMP_FRAC are read-only (set by hardware) */
    hw_model_set_readonly(&m->hw, MAX_REG_TEMP_INT);
    hw_model_set_readonly(&m->hw, MAX_REG_TEMP_FRAC);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, MAX_REG_MODE_CONFIG, max30102_on_mode_config);
    hw_model_set_on_write(&m->hw, MAX_REG_FIFO_WR_PTR, max30102_on_fifo_ptr);
    hw_model_set_on_write(&m->hw, MAX_REG_FIFO_RD_PTR, max30102_on_fifo_ptr);
    hw_model_set_on_write(&m->hw, MAX_REG_SPO2_CONFIG, max30102_on_spo2_config);
    hw_model_set_on_write(&m->hw, MAX_REG_TEMP_CONFIG, max30102_on_temp_config);

    /* Apply reset values */
    hw_model_reset(&m->hw);

    /* p.12: "On power-up ... a power-ready interrupt is triggered" */
    m->hw.regs[MAX_REG_INT_STATUS_1] = MAX_INT1_PWR_RDY;

    /* Default simulated die temperature: 25.0 C */
    m->true_temp_c = 25.0f;
}

/* --- Helper: inject FIFO samples with realistic ADC values ---
 *
 * In SpO2 mode each sample is 6 bytes: RED[17:0] + IR[17:0], each
 * left-justified in 3 bytes (bits 17:0 in lower 18 bits).
 * Typical ADC range for a finger at rest with ~6.4 mA LED:
 *   RED ~50000..120000, IR ~70000..150000  (18-bit, max 262143)
 *
 * A full FIFO follows p.14 and p.18. Rollover off: "samples are not pushed on
 * to the FIFO, samples are lost", and OVF_COUNTER counts them. Rollover on:
 * "the FIFO address rolls over to zero and the FIFO continues to fill with new
 * data", over the oldest samples. The manual says no more than that about this
 * case: FIFO_RD_PTR stays where it is here, and OVF_COUNTER still counts, which
 * is only the model's reading of "counts the number of samples lost".
 */
__attribute__((unused))
static void max30102_model_inject_sample(max30102_model_t *m,
                                         uint32_t red, uint32_t ir)
{
    if (m->fifo_unread == MAX_FIFO_DEPTH) {
        if (m->hw.regs[MAX_REG_OVF_COUNTER] < MAX_OVF_MAX)
            m->hw.regs[MAX_REG_OVF_COUNTER]++;
        if (!(m->hw.regs[MAX_REG_FIFO_CONFIG] & MAX_FIFO_ROLLOVER_EN))
            return;
    } else {
        m->fifo_unread++;
    }

    uint8_t idx = m->fifo_wr;
    uint16_t base = (uint16_t)(idx * 6u);

    /* RED channel (18-bit, MSB first) */
    m->fifo[base + 0] = (uint8_t)((red >> 16) & 0x03u);
    m->fifo[base + 1] = (uint8_t)((red >> 8) & 0xFFu);
    m->fifo[base + 2] = (uint8_t)(red & 0xFFu);

    /* IR channel (18-bit, MSB first) */
    m->fifo[base + 3] = (uint8_t)((ir >> 16) & 0x03u);
    m->fifo[base + 4] = (uint8_t)((ir >> 8) & 0xFFu);
    m->fifo[base + 5] = (uint8_t)(ir & 0xFFu);

    m->fifo_wr = (uint8_t)((m->fifo_wr + 1) & 0x1Fu);
    m->hw.regs[MAX_REG_FIFO_WR_PTR] = m->fifo_wr;
}

/* Helper: set simulated die temperature */
__attribute__((unused))
static void max30102_model_set_temperature(max30102_model_t *m, float temp_c)
{
    m->true_temp_c = temp_c;
}

/* Helper: delay callback, the model's time base */
__attribute__((unused))
static void max30102_model_delay(void *ctx, uint32_t ms)
{
    max30102_model_t *m = (max30102_model_t *)ctx;

    hw_model_delay_ms(&m->hw, ms);
    if (m->temp_ms_left == 0)
        return;
    if (ms < m->temp_ms_left) {
        m->temp_ms_left -= ms;
    } else {
        m->temp_ms_left = 0;
        max30102_temp_done(m);
    }
}

#endif /* HW_MODEL_MAX30102_H */
