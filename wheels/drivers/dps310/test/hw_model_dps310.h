// SPDX-License-Identifier: MIT
/*
 * hw_model_dps310.h — datasheet-accurate register model for DPS310
 *
 * Provides write_reg/read_regs callbacks backed by a register array
 * with power-on-reset defaults. Use in test_hw_model_dps310.c
 * for round-trip verification against the real driver.
 *
 * Dummy calibration coefficients (chosen for tractable test math):
 *   c0  = 0       c1  = 100
 *   c00 = 100000  c10 = 1000
 *   c01 = c11 = c20 = c21 = c30 = 0
 *
 * With 1x oversampling (kT = kP = 524288):
 *   T_comp = c1 * T_raw / kT  = 100 * T_raw / 524288
 *   P_comp = c00 + c10 * P_raw / kP = 100000 + 1000 * P_raw / 524288
 *
 * Start-up (Table 8, p.10): after power-on or a soft reset SENSOR_RDY rises
 * at 12 ms and COEF_RDY at 40 ms, and the coefficient registers read 0
 * until COEF_RDY. Only the delay callback lets time pass.
 * hw_model_power_on() starts there; hw_model_reset() is a chip already
 * past it.
 *
 * Figure 4 (p.11): "No chip access" for the first 2.5 ms of start-up, so
 * every transfer fails then.
 *
 * Table 16 (p.30): a command measurement takes its time. MEAS_CTRL stays set,
 * the result registers keep their old value, and only at the end does the
 * new result appear, the ready flag rise and the mode return to Standby.
 * hw_model_inject_*() set what the next measurement will find.
 */
#ifndef HW_MODEL_DPS310_H
#define HW_MODEL_DPS310_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

/* Table 8 (p.10): maximum times after power-on or soft reset */
#define HW_SENSOR_RDY_MS 12u
#define HW_COEF_RDY_MS   40u

/* Figure 4 (p.11): 2.5 ms, rounded up to the whole ms the model counts */
#define HW_NO_ACCESS_MS  3u

static uint8_t  hw_regs[HW_REG_COUNT];
static int      hw_write_count;
static int      hw_read_count;
static uint32_t hw_uptime_ms;   /* time since power-on or soft reset */
static uint32_t hw_meas_left_ms; /* time left in the running measurement */
static int32_t  hw_true_prs;    /* raw value the next measurement finds */
static int32_t  hw_true_tmp;

/* Table 16 (p.30), rounded up to whole ms. Index = PM_PRC or TM_PRC, and
 * temperature "is similar with pressure". */
static const uint8_t hw_meas_ms[8] = { 4, 6, 9, 15, 28, 54, 105, 207 };

/*
 * Encode a signed 24-bit value into 3 register bytes (big-endian).
 * reg_start is the MSB register address (e.g. 0x00 for PSR, 0x03 for TMP).
 */
static void hw_encode_24(uint8_t reg_start, int32_t val)
{
    uint32_t u = (uint32_t)val & 0x00FFFFFFu;
    hw_regs[reg_start]     = (uint8_t)((u >> 16) & 0xFFu);
    hw_regs[(uint8_t)(reg_start + 1u)] = (uint8_t)((u >> 8) & 0xFFu);
    hw_regs[(uint8_t)(reg_start + 2u)] = (uint8_t)(u & 0xFFu);
}

static void hw_load_coefficients(void)
{
    /*
     * Calibration coefficients in registers 0x10-0x21.
     *
     * c0  = 0     (12-bit)
     * c1  = 100   (12-bit): 0x064
     * c00 = 100000 (20-bit): 0x186A0
     * c10 = 1000  (20-bit): 0x003E8
     * c01..c30 = 0 (16-bit each)
     *
     * Register encoding:
     *   0x10: c0[11:4]  = 0x00
     *   0x11: c0[3:0]<<4 | c1[11:8] = 0x00
     *   0x12: c1[7:0]   = 0x64
     *   0x13: c00[19:12] = 0x18
     *   0x14: c00[11:4]  = 0x6A
     *   0x15: c00[3:0]<<4 | c10[19:16] = 0x00
     *   0x16: c10[15:8]  = 0x03
     *   0x17: c10[7:0]   = 0xE8
     *   0x18-0x21: all zeros (c01, c11, c20, c21, c30)
     */
    hw_regs[0x10] = 0x00;  /* c0[11:4] */
    hw_regs[0x11] = 0x00;  /* c0[3:0]<<4 | c1[11:8] */
    hw_regs[0x12] = 0x64;  /* c1[7:0] = 100 */
    hw_regs[0x13] = 0x18;  /* c00[19:12] */
    hw_regs[0x14] = 0x6A;  /* c00[11:4] */
    hw_regs[0x15] = 0x00;  /* c00[3:0]<<4 | c10[19:16] */
    hw_regs[0x16] = 0x03;  /* c10[15:8] */
    hw_regs[0x17] = 0xE8;  /* c10[7:0] */
    /* 0x18-0x21 remain zero (c01..c30 = 0) */

    /* TMP_COEF_SRCE: XXH in the manual, set per part. MEMS here; tests
     * change it. */
    hw_regs[0x28] = 0x80;
}

/* Figure 3 (p.11): a command measurement goes back to Standby */
static void hw_end_measurement(void)
{
    uint8_t ctrl = (uint8_t)(hw_regs[0x08] & 0x07u);

    if (ctrl == 0x02u)
    {
        hw_encode_24(0x03, hw_true_tmp);
        hw_regs[0x08] |= 0x20u;  /* TMP_RDY */
    }
    else if (ctrl == 0x01u)
    {
        hw_encode_24(0x00, hw_true_prs);
        hw_regs[0x08] |= 0x10u;  /* PRS_RDY */
    }
    hw_regs[0x08] &= (uint8_t)~0x07u;
}

/* Let time pass: the ready bits rise and the coefficients appear */
static void hw_advance(uint32_t ms)
{
    hw_uptime_ms += ms;
    if (hw_meas_left_ms != 0u)
    {
        if (ms >= hw_meas_left_ms)
        {
            hw_meas_left_ms = 0u;
            hw_end_measurement();
        }
        else
        {
            hw_meas_left_ms -= ms;
        }
    }
    if (hw_uptime_ms >= HW_SENSOR_RDY_MS)
    {
        hw_regs[0x08] |= 0x40u;  /* SENSOR_RDY */
    }
    if (hw_uptime_ms >= HW_COEF_RDY_MS && !(hw_regs[0x08] & 0x80u))
    {
        hw_load_coefficients();
        hw_regs[0x08] |= 0x80u;  /* COEF_RDY */
    }
}

/* Shared by power-on and soft reset: reset values, ready bits low */
static void hw_start_up(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_uptime_ms  = 0;
    hw_meas_left_ms = 0;
    hw_regs[0x0D] = 0x10;  /* Product ID */
}

/* Cold boot: nothing is ready until the delay callback lets time pass */
static void hw_model_power_on(void)
{
    hw_write_count = 0;
    hw_read_count  = 0;
    hw_true_prs    = 0;
    hw_true_tmp    = 0;
    hw_start_up();
}

/* Chip past start-up: MEAS_CFG at its reset value C0H (p.32) */
static void hw_model_reset(void)
{
    hw_model_power_on();
    hw_advance(HW_COEF_RDY_MS);
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    if (hw_uptime_ms < HW_NO_ACCESS_MS)
    {
        return false;
    }
    hw_write_count++;

    /* RESET: '1001' in SOFT_RST bits 3:0 reruns the power-on sequence (p.36) */
    if (reg == 0x0Cu)
    {
        if ((val & 0x0Fu) == 0x09u)
        {
            hw_start_up();
        }
        return true;
    }

    /* MEAS_CFG: only MEAS_CTRL is writable (p.32) */
    if (reg == 0x08u)
    {
        uint8_t ctrl = (uint8_t)(val & 0x07u);

        hw_regs[0x08] = (uint8_t)((hw_regs[0x08] & 0xF8u) | ctrl);
        hw_meas_left_ms = 0u;
        if (ctrl == 0x02u)  /* temperature command */
        {
            hw_meas_left_ms = hw_meas_ms[hw_regs[0x07] & 0x07u];
        }
        if (ctrl == 0x01u)  /* pressure command */
        {
            hw_meas_left_ms = hw_meas_ms[hw_regs[0x06] & 0x07u];
        }
        return true;
    }

    /* Table 15 (p.25): nothing else has an (rw) field. Bit 7 of PRS_CFG is
     * "-". */
    if (reg == 0x06u)
    {
        hw_regs[0x06] = (uint8_t)(val & 0x7Fu);
    }
    else if (reg == 0x07u || reg == 0x09u)
    {
        hw_regs[reg] = val;
    }
    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    if (hw_uptime_ms < HW_NO_ACCESS_MS)
    {
        return false;
    }
    hw_read_count++;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t r = (uint8_t)(reg + i);
        buf[i] = hw_regs[r];
        /* p.32: reading a result clears its ready flag */
        if (r <= 0x02u)
        {
            hw_regs[0x08] &= (uint8_t)~0x10u;  /* PRS_RDY */
        }
        else if (r <= 0x05u)
        {
            hw_regs[0x08] &= (uint8_t)~0x20u;  /* TMP_RDY */
        }
    }
    return true;
}

/* Delay callback: the only way time passes in the model */
__attribute__((unused))
static void hw_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ctx;
    hw_advance(ms);
}

/*
 * Inject a temperature value by computing the raw ADC value.
 *
 * With dummy coefficients (c0=0, c1=100, kT=524288):
 *   T_comp = c1 * T_raw / kT = 100 * T_raw / 524288
 *   T_raw  = T_comp * 524288 / 100 = T_comp * 5242.88
 *
 * The next temperature measurement writes it to TMP registers 0x03-0x05.
 */
__attribute__((unused))
static void hw_model_inject_temperature(float temp_c)
{
    hw_true_tmp = (int32_t)(temp_c * 524288.0f / 100.0f);
}

/*
 * Inject a pressure value by computing the raw ADC value.
 *
 * With dummy coefficients (c00=100000, c10=1000, kP=524288, cross terms=0):
 *   P_comp = 100000 + 1000 * P_raw / 524288
 *   P_raw  = (P_comp - 100000) * 524288 / 1000
 *
 * The next pressure measurement writes it to PSR registers 0x00-0x02.
 */
__attribute__((unused))
static void hw_model_inject_pressure(float press_pa)
{
    hw_true_prs = (int32_t)((press_pa - 100000.0f) * 524288.0f / 1000.0f);
}

#endif /* HW_MODEL_DPS310_H */
