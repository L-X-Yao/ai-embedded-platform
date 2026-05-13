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
 */
#ifndef HW_MODEL_DPS310_H
#define HW_MODEL_DPS310_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HW_REG_COUNT 256

static uint8_t hw_regs[HW_REG_COUNT];
static int     hw_write_count;
static int     hw_read_count;

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

static void hw_model_reset(void)
{
    memset(hw_regs, 0, sizeof(hw_regs));
    hw_write_count = 0;
    hw_read_count  = 0;

    /* POR defaults */
    hw_regs[0x0D] = 0x10;  /* Product ID */

    /* MEAS_CFG: assert TMP_RDY + PRS_RDY so reads don't timeout */
    hw_regs[0x08] = 0x30;

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
}

__attribute__((unused))
static bool hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t val)
{
    (void)ctx; (void)dev_addr;
    hw_write_count++;

    /* RESET register: soft reset restores measurement-ready flags */
    if (reg == 0x0Cu && val == 0x89u)
    {
        hw_regs[0x08] = 0x30;  /* re-assert TMP_RDY + PRS_RDY */
        return true;
    }

    hw_regs[reg] = val;

    /* MEAS_CFG write: simulate measurement complete immediately */
    if (reg == 0x08u)
    {
        if (val & 0x02u)  /* temperature command */
        {
            hw_regs[0x08] |= 0x20u;  /* TMP_RDY */
        }
        if (val & 0x01u)  /* pressure command */
        {
            hw_regs[0x08] |= 0x10u;  /* PRS_RDY */
        }
    }

    return true;
}

__attribute__((unused))
static bool hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)ctx; (void)dev_addr;
    hw_read_count++;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = hw_regs[(uint8_t)(reg + i)];
    }
    return true;
}

/*
 * Inject a temperature value by computing the raw ADC value.
 *
 * With dummy coefficients (c0=0, c1=100, kT=524288):
 *   T_comp = c1 * T_raw / kT = 100 * T_raw / 524288
 *   T_raw  = T_comp * 524288 / 100 = T_comp * 5242.88
 *
 * Writes the 24-bit big-endian value to TMP registers 0x03-0x05.
 */
__attribute__((unused))
static void hw_model_inject_temperature(float temp_c)
{
    int32_t raw = (int32_t)(temp_c * 524288.0f / 100.0f);
    hw_encode_24(0x03, raw);
}

/*
 * Inject a pressure value by computing the raw ADC value.
 *
 * With dummy coefficients (c00=100000, c10=1000, kP=524288, cross terms=0):
 *   P_comp = 100000 + 1000 * P_raw / 524288
 *   P_raw  = (P_comp - 100000) * 524288 / 1000
 *
 * Writes the 24-bit big-endian value to PSR registers 0x00-0x02.
 */
__attribute__((unused))
static void hw_model_inject_pressure(float press_pa)
{
    int32_t raw = (int32_t)((press_pa - 100000.0f) * 524288.0f / 1000.0f);
    hw_encode_24(0x00, raw);
}

#endif /* HW_MODEL_DPS310_H */
