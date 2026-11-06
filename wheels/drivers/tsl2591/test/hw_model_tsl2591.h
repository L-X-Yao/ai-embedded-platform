// SPDX-License-Identifier: MIT
/*
 * hw_model_tsl2591.h -- AMS TSL2591 ambient light sensor hardware model, after
 * the datasheet of Apr. 2013, ams163.5
 *
 * Models TSL2591 register behavior:
 * - Command byte prefix: all register accesses use (0xA0 | reg)
 * - ENABLE register (0x00): PON[0], AEN[1], AIEN[4], SAI[6], NPIEN[7]
 * - CONFIG register (0x01): itime[2:0], gain[5:4], SRESET[7]
 * - PID register (0x11): package 00, read-only
 * - ID register (0x12): fixed 0x50 (read-only)
 * - STATUS register (0x13): AVALID[0], AINT[4] (read-only)
 * - Channel data (0x14-0x17): C0L, C0H, C1L, C1H (read-only, little-endian)
 *
 * Time does not pass by itself: tsl2591_model_end_integration() stands for the
 * end of one integration cycle. Only then does the light set with
 * tsl2591_model_set_channels() reach the data registers and AVALID go high.
 *
 * The driver sends (0xA0 | reg) as the register address to the I2C HAL,
 * so the hw_model indexes by command-byte-prefixed address.
 */

#ifndef HW_MODEL_TSL2591_H
#define HW_MODEL_TSL2591_H

#include "hw_model.h"

/* command-byte-prefixed register addresses (as seen by the HAL) */
#define TSL_CMD(reg)  ((uint8_t)(0xA0u | (reg)))

#define TSL_ENABLE   TSL_CMD(0x00u)
#define TSL_CONFIG   TSL_CMD(0x01u)
#define TSL_PID      TSL_CMD(0x11u)
#define TSL_ID       TSL_CMD(0x12u)
#define TSL_STATUS   TSL_CMD(0x13u)
#define TSL_C0DATAL  TSL_CMD(0x14u)
#define TSL_C0DATAH  TSL_CMD(0x15u)
#define TSL_C1DATAL  TSL_CMD(0x16u)
#define TSL_C1DATAH  TSL_CMD(0x17u)

#define TSL_ENABLE_ALS   0x03u  /* PON and AEN */
#define TSL_STATUS_AVALID 0x01u
#define TSL_CONFIG_SRESET 0x80u

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_ch0;    /* simulated channel 0 (visible + IR) */
    uint16_t        true_ch1;    /* simulated channel 1 (IR only) */
} tsl2591_model_t;

/* Callback: ENABLE register write. p.16: AVALID says a cycle has finished
 * "since the AEN bit was asserted", so it falls when the ALS starts again or
 * stops, and stays when only an interrupt enable changes. */
static void tsl2591_model_on_enable(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    tsl2591_model_t *m = (tsl2591_model_t *)user;

    bool was_on = (old_val & TSL_ENABLE_ALS) == TSL_ENABLE_ALS;
    bool is_on  = (new_val & TSL_ENABLE_ALS) == TSL_ENABLE_ALS;
    if (!was_on || !is_on) {
        m->hw.regs[TSL_STATUS] &= (uint8_t)~TSL_STATUS_AVALID;
    }
}

/* Callback: CONFIG register write. p.13: SRESET resets the device "equivalent
 * to a power-on reset" and "is self-clearing". */
static void tsl2591_model_on_config(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    tsl2591_model_t *m = (tsl2591_model_t *)user;

    if (new_val & TSL_CONFIG_SRESET) {
        hw_model_reset(&m->hw);
    }
}

static void tsl2591_model_init(tsl2591_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* ID register: fixed 0x50, read-only */
    hw_model_set_reset_value(&m->hw, TSL_ID, 0x50u);
    hw_model_set_readonly(&m->hw, TSL_ID);

    /* PID register: package 00, read-only (p.16) */
    hw_model_set_readonly(&m->hw, TSL_PID);

    /* STATUS register: read-only */
    hw_model_set_readonly(&m->hw, TSL_STATUS);

    /* Channel data registers: read-only */
    hw_model_set_readonly(&m->hw, TSL_C0DATAL);
    hw_model_set_readonly(&m->hw, TSL_C0DATAH);
    hw_model_set_readonly(&m->hw, TSL_C1DATAL);
    hw_model_set_readonly(&m->hw, TSL_C1DATAH);

    /* ENABLE register write callback */
    hw_model_set_on_write(&m->hw, TSL_ENABLE, tsl2591_model_on_enable);

    /* ENABLE writable bits: PON[0], AEN[1], AIEN[4], SAI[6], NPIEN[7] (p.12) */
    hw_model_set_writable_mask(&m->hw, TSL_ENABLE, 0xD3u);

    /* CONFIG writable bits: itime[2:0], gain[5:4], SRESET[7] (p.13) */
    hw_model_set_writable_mask(&m->hw, TSL_CONFIG, 0xB7u);
    hw_model_set_on_write(&m->hw, TSL_CONFIG, tsl2591_model_on_config);

    /* Default simulated data */
    m->true_ch0 = 500;   /* visible + IR */
    m->true_ch1 = 100;   /* IR only */

    hw_model_reset(&m->hw);
}

/* Helper: set the light on the sensor. It reaches the data registers at the
 * end of the next integration cycle. */
__attribute__((unused))
static void tsl2591_model_set_channels(tsl2591_model_t *m,
                                        uint16_t ch0, uint16_t ch1)
{
    m->true_ch0 = ch0;
    m->true_ch1 = ch1;
}

/* Helper: one integration cycle ends. p.2: "Upon completion of the conversion
 * cycle, the conversion result is transferred to the Channel 0 and Channel 1
 * data registers". p.13: MAX COUNT is 37888 at 100 ms, 65535 otherwise. */
__attribute__((unused))
static void tsl2591_model_end_integration(tsl2591_model_t *m)
{
    if ((m->hw.regs[TSL_ENABLE] & TSL_ENABLE_ALS) != TSL_ENABLE_ALS) return;

    uint16_t max = ((m->hw.regs[TSL_CONFIG] & 0x07u) == 0u) ? 37888u : 65535u;
    uint16_t ch0 = (m->true_ch0 > max) ? max : m->true_ch0;
    uint16_t ch1 = (m->true_ch1 > max) ? max : m->true_ch1;

    m->hw.regs[TSL_C0DATAL] = (uint8_t)(ch0 & 0xFF);
    m->hw.regs[TSL_C0DATAH] = (uint8_t)(ch0 >> 8);
    m->hw.regs[TSL_C1DATAL] = (uint8_t)(ch1 & 0xFF);
    m->hw.regs[TSL_C1DATAH] = (uint8_t)(ch1 >> 8);
    m->hw.regs[TSL_STATUS] |= TSL_STATUS_AVALID;
}

#endif /* HW_MODEL_TSL2591_H */
