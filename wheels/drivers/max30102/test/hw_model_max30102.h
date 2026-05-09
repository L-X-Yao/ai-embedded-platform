/*
 * hw_model_max30102.h -- MAX30102 register-based hardware model
 *
 * Models Maxim MAX30102 (datasheet 19-6368 Rev 1) register behavior:
 * - Part ID 0x15 at register 0xFF (read-only)
 * - Rev ID at register 0xFE (read-only, default 0x01)
 * - Software reset via MODE_CONFIG (0x09) bit 6, auto-clears
 * - FIFO with 32 slots, 3 bytes per channel (18-bit ADC, left-justified)
 * - FIFO_WR_PTR (0x04), OVF_COUNTER (0x05), FIFO_RD_PTR (0x06)
 * - FIFO_DATA (0x07): burst-read returns sample bytes, advances RD_PTR
 * - Die temperature: TEMP_INT (0x1F, signed), TEMP_FRAC (0x20, 4-bit)
 * - Temperature one-shot trigger via TEMP_CONFIG (0x21)
 * - LED1_PA (0x0C), LED2_PA (0x0D): 0.2 mA per step
 * - MODE_CONFIG (0x09): mode[2:0], SHDN bit 7, RESET bit 6
 * - SPO2_CONFIG (0x0A): ADC_RANGE[6:5], SR[4:2], PW[1:0]
 * - FIFO_CONFIG (0x08): SMP_AVE[7:5], FIFO_ROLLOVER[4], FIFO_A_FULL[3:0]
 */

#ifndef HW_MODEL_MAX30102_H
#define HW_MODEL_MAX30102_H

#include "hw_model.h"

/* --- MAX30102 register addresses (from datasheet Table 1) --- */
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
#define MAX_REG_TEMP_INT      0x1Fu
#define MAX_REG_TEMP_FRAC     0x20u
#define MAX_REG_TEMP_CONFIG   0x21u
#define MAX_REG_REV_ID        0xFEu
#define MAX_REG_PART_ID       0xFFu

/* --- FIFO depth --- */
#define MAX_FIFO_DEPTH        32u

/* --- Model structure --- */
typedef struct {
    hw_reg_model_t  hw;
    /* FIFO storage: up to 32 samples, each has RED (3 bytes) + IR (3 bytes) */
    uint8_t         fifo[MAX_FIFO_DEPTH * 6];
    uint8_t         fifo_wr;        /* 5-bit write pointer   */
    uint8_t         fifo_rd;        /* 5-bit read pointer    */
    uint8_t         fifo_mode;      /* 0x02=HR, 0x03=SpO2    */
    /* Simulated die temperature */
    float           true_temp_c;
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

    /* Reset bit (bit 6) auto-clears after reset */
    if (new_val & 0x40u) {
        /* Clear FIFO pointers */
        m->hw.regs[MAX_REG_FIFO_WR_PTR] = 0;
        m->hw.regs[MAX_REG_OVF_COUNTER] = 0;
        m->hw.regs[MAX_REG_FIFO_RD_PTR] = 0;
        m->fifo_wr = 0;
        m->fifo_rd = 0;
        /* Clear the reset bit (self-clearing) */
        m->hw.regs[MAX_REG_MODE_CONFIG] = (uint8_t)(new_val & ~0x40u);
        m->reset_count++;
    }

    /* Track mode for FIFO read byte count */
    m->fifo_mode = new_val & 0x07u;
}

/* --- Write callback: TEMP_CONFIG triggers one-shot temperature --- */
__attribute__((unused))
static void max30102_on_temp_config(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    max30102_model_t *m = (max30102_model_t *)user;

    if (new_val & 0x01u) {
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

        /* Auto-clear the trigger bit */
        m->hw.regs[MAX_REG_TEMP_CONFIG] = 0x00u;
        m->temp_trigger_count++;
    }
}

/* --- Custom read callback: FIFO_DATA returns sample bytes --- */

/* Override the default read_regs to handle FIFO burst reads.
 * When reading from FIFO_DATA (0x07), each byte comes from the
 * FIFO storage and auto-advances the read pointer after each
 * complete sample. */
__attribute__((unused))
static bool max30102_model_read_regs(void *ctx, uint8_t dev_addr,
                                     uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    max30102_model_t *m = (max30102_model_t *)ctx;

    if (reg == MAX_REG_FIFO_DATA) {
        /* FIFO burst read: return sample data from internal FIFO */
        uint8_t bytes_per_sample = (m->fifo_mode == 0x03u) ? 6u : 3u;

        for (uint16_t i = 0; i < len; i++) {
            uint16_t fifo_byte_idx =
                (uint16_t)(m->fifo_rd * 6u) + (i % bytes_per_sample);
            if (fifo_byte_idx < sizeof(m->fifo))
                buf[i] = m->fifo[fifo_byte_idx];
            else
                buf[i] = 0x00u;

            /* Advance read pointer after each complete sample */
            if (((i + 1) % bytes_per_sample) == 0) {
                m->fifo_rd = (uint8_t)((m->fifo_rd + 1) & 0x1Fu);
                m->hw.regs[MAX_REG_FIFO_RD_PTR] = m->fifo_rd;
            }
        }
        return true;
    }

    /* Default register read */
    return hw_model_i2c_read_regs(ctx, dev_addr, reg, buf, len);
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

    /* Interrupt status registers are read-clear (read-only from write) */
    hw_model_set_readonly(&m->hw, MAX_REG_INT_STATUS_1);
    hw_model_set_readonly(&m->hw, MAX_REG_INT_STATUS_2);

    /* FIFO_DATA is read-only (writes are ignored) */
    hw_model_set_readonly(&m->hw, MAX_REG_FIFO_DATA);

    /* TEMP_INT and TEMP_FRAC are read-only (set by hardware) */
    hw_model_set_readonly(&m->hw, MAX_REG_TEMP_INT);
    hw_model_set_readonly(&m->hw, MAX_REG_TEMP_FRAC);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, MAX_REG_MODE_CONFIG, max30102_on_mode_config);
    hw_model_set_on_write(&m->hw, MAX_REG_TEMP_CONFIG, max30102_on_temp_config);

    /* Apply reset values */
    hw_model_reset(&m->hw);

    /* Default simulated die temperature: 25.0 C */
    m->true_temp_c = 25.0f;
    m->fifo_mode = 0x03u;  /* default SpO2 mode */
}

/* --- Helper: inject FIFO samples with realistic ADC values ---
 *
 * In SpO2 mode each sample is 6 bytes: RED[17:0] + IR[17:0], each
 * left-justified in 3 bytes (bits 17:0 in lower 18 bits).
 * Typical ADC range for a finger at rest with ~6.4 mA LED:
 *   RED ~50000..120000, IR ~70000..150000  (18-bit, max 262143)
 */
__attribute__((unused))
static void max30102_model_inject_sample(max30102_model_t *m,
                                         uint32_t red, uint32_t ir)
{
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

/* Helper: delay callback (no-op) */
__attribute__((unused))
static void max30102_model_delay(void *ctx, uint32_t ms)
{
    (void)ctx; (void)ms;
}

#endif /* HW_MODEL_MAX30102_H */
