/*
 * hw_model_tsl2591.h -- AMS TSL2591 ambient light sensor hardware model
 *
 * Models TSL2591 register behavior:
 * - Command byte prefix: all register accesses use (0xA0 | reg)
 * - ENABLE register (0x00): PON[0], AEN[1]
 * - CONFIG register (0x01): gain[5:4], itime[2:0]
 * - ID register (0x12): fixed 0x50 (read-only)
 * - STATUS register (0x13): AVALID[0], AINT[4] (read-only)
 * - Channel data (0x14-0x17): C0L, C0H, C1L, C1H (read-only, little-endian)
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
#define TSL_ID       TSL_CMD(0x12u)
#define TSL_STATUS   TSL_CMD(0x13u)
#define TSL_C0DATAL  TSL_CMD(0x14u)
#define TSL_C0DATAH  TSL_CMD(0x15u)
#define TSL_C1DATAL  TSL_CMD(0x16u)
#define TSL_C1DATAH  TSL_CMD(0x17u)

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_ch0;    /* simulated channel 0 (visible + IR) */
    uint16_t        true_ch1;    /* simulated channel 1 (IR only) */
} tsl2591_model_t;

/* Callback: ENABLE register write -- when AEN+PON, data becomes available */
static void tsl2591_model_on_enable(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    tsl2591_model_t *m = (tsl2591_model_t *)user;

    /* When AEN (bit 1) and PON (bit 0) are both set, populate channel data */
    if ((new_val & 0x03u) == 0x03u) {
        m->hw.regs[TSL_C0DATAL] = (uint8_t)(m->true_ch0 & 0xFF);
        m->hw.regs[TSL_C0DATAH] = (uint8_t)(m->true_ch0 >> 8);
        m->hw.regs[TSL_C1DATAL] = (uint8_t)(m->true_ch1 & 0xFF);
        m->hw.regs[TSL_C1DATAH] = (uint8_t)(m->true_ch1 >> 8);
        /* Set AVALID in STATUS */
        m->hw.regs[TSL_STATUS] = 0x01u;
    } else {
        m->hw.regs[TSL_STATUS] = 0x00u;
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

    /* STATUS register: read-only */
    hw_model_set_readonly(&m->hw, TSL_STATUS);

    /* Channel data registers: read-only */
    hw_model_set_readonly(&m->hw, TSL_C0DATAL);
    hw_model_set_readonly(&m->hw, TSL_C0DATAH);
    hw_model_set_readonly(&m->hw, TSL_C1DATAL);
    hw_model_set_readonly(&m->hw, TSL_C1DATAH);

    /* ENABLE register write callback */
    hw_model_set_on_write(&m->hw, TSL_ENABLE, tsl2591_model_on_enable);

    /* ENABLE register writable bits: PON[0], AEN[1], AIEN[4], NPIEN[7] */
    hw_model_set_writable_mask(&m->hw, TSL_ENABLE, 0x93u);

    /* CONFIG register writable bits: itime[2:0], gain[5:4] */
    hw_model_set_writable_mask(&m->hw, TSL_CONFIG, 0x37u);

    /* Default simulated data */
    m->true_ch0 = 500;   /* visible + IR */
    m->true_ch1 = 100;   /* IR only */

    hw_model_reset(&m->hw);
}

/* Helper: set simulated channel values */
static void tsl2591_model_set_channels(tsl2591_model_t *m,
                                        uint16_t ch0, uint16_t ch1)
{
    m->true_ch0 = ch0;
    m->true_ch1 = ch1;

    /* If sensor is enabled, update data registers immediately */
    if ((m->hw.regs[TSL_ENABLE] & 0x03u) == 0x03u) {
        m->hw.regs[TSL_C0DATAL] = (uint8_t)(ch0 & 0xFF);
        m->hw.regs[TSL_C0DATAH] = (uint8_t)(ch0 >> 8);
        m->hw.regs[TSL_C1DATAL] = (uint8_t)(ch1 & 0xFF);
        m->hw.regs[TSL_C1DATAH] = (uint8_t)(ch1 >> 8);
    }
}

#endif /* HW_MODEL_TSL2591_H */
