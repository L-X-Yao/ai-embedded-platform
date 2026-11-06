// SPDX-License-Identifier: MIT
/*
 * hw_model_hmc5883l.h — HMC5883L hardware register model from datasheet
 *
 * Models HMC5883L register behavior:
 * - ID registers at 0x0A-0x0C: 'H','4','3' (read-only)
 * - Config A at 0x00: data output rate [4:2], reset 0x10
 * - Config B at 0x01: gain [7:5], reset 0x20
 * - Mode at 0x02: measurement mode [1:0], reset 0x01
 * - Data registers 0x03..0x08: X_H,X_L,Z_H,Z_L,Y_H,Y_L (read-only, big-endian)
 * - Status at 0x09: RDY and LOCK (read-only)
 * - Continuous mode: after a write to Mode, CRA or CRB the first set comes
 *   after 2/fDO and the next ones every 1/fDO. Time passes in
 *   hmc5883l_model_advance_ms and in the delay_ms callback. A gain change
 *   takes effect from the second measurement
 * - A set is held back while the data is locked by a partial read
 * - An axis outside -2048..2047 reads -4096
 *
 * Single-measurement mode is not modelled: the driver does not use it. RDY
 * follows p16 unless rdy_cleared_by_read is set, which gives the reading of
 * the Single-Measurement row of Table 12 (p14) instead.
 */

#ifndef HW_MODEL_HMC5883L_H
#define HW_MODEL_HMC5883L_H

#include "hw_model.h"

/* --- HMC5883L register addresses --- */
#define HMC5883L_REG_CONFIG_A  0x00u
#define HMC5883L_REG_CONFIG_B  0x01u
#define HMC5883L_REG_MODE      0x02u
#define HMC5883L_REG_DATA_X_H  0x03u
#define HMC5883L_REG_STATUS    0x09u
#define HMC5883L_REG_ID_A      0x0Au
#define HMC5883L_REG_ID_B      0x0Bu
#define HMC5883L_REG_ID_C      0x0Cu

/* Status bits, Table 17 p16 */
#define HMC5883L_STATUS_RDY    0x01u
#define HMC5883L_STATUS_LOCK   0x02u

#define HMC5883L_DATA_ALL_READ 0x3Fu   /* one bit per data register */
#define HMC5883L_DATA_OVERFLOW (-4096)

typedef struct {
    hw_reg_model_t  hw;
    int16_t         true_x;     /* simulated field: X counts at gain 1090 */
    int16_t         true_y;     /* simulated field: Y counts at gain 1090 */
    int16_t         true_z;     /* simulated field: Z counts at gain 1090 */
    int             measure_count;
    uint32_t        next_set_us;  /* time to the next measurement, 0 = none */
    uint8_t         gain_code;    /* GN the next measurement uses */
    uint8_t         data_read;    /* data registers read since the last set */
    bool            rdy_cleared_by_read;
} hmc5883l_model_t;

/* 1/fDO for each DO code, Table 5 p12. Code 7 is reserved. */
static uint32_t hmc5883l_model_period_us(const hmc5883l_model_t *m)
{
    static const uint32_t period_us[8] = {
        1333334u, 666667u, 333334u, 133334u, 66667u, 33334u, 13334u, 66667u,
    };
    return period_us[(m->hw.regs[HMC5883L_REG_CONFIG_A] >> 2) & 0x07u];
}

/* One axis at the gain in use. Table 9 p13 gives every gain the output range
 * -2048 to 2047, and p15 makes anything past it read -4096. */
static int16_t hmc5883l_model_counts(const hmc5883l_model_t *m, int16_t field)
{
    static const int32_t lsb_per_gauss[8] = {
        1370, 1090, 820, 660, 440, 390, 330, 230,
    };
    int32_t counts = (int32_t)field * lsb_per_gauss[m->gain_code] / 1090;
    if (counts < -2048 || counts > 2047) return HMC5883L_DATA_OVERFLOW;
    return (int16_t)counts;
}

/* Populate data registers from true values (big-endian: X, Z, Y order) */
static void hmc5883l_model_set_data(hmc5883l_model_t *m)
{
    int16_t x = hmc5883l_model_counts(m, m->true_x);
    int16_t y = hmc5883l_model_counts(m, m->true_y);
    int16_t z = hmc5883l_model_counts(m, m->true_z);

    m->hw.regs[0x03] = (uint8_t)((uint16_t)x >> 8);
    m->hw.regs[0x04] = (uint8_t)(x & 0xFF);
    m->hw.regs[0x05] = (uint8_t)((uint16_t)z >> 8);
    m->hw.regs[0x06] = (uint8_t)(z & 0xFF);
    m->hw.regs[0x07] = (uint8_t)((uint16_t)y >> 8);
    m->hw.regs[0x08] = (uint8_t)(y & 0xFF);
}

/* End of one measurement. p16: while LOCK is set "any new data will not be
 * placed in these register". p13: the first measurement after a gain change
 * keeps the old gain, so the new one applies from the next. */
static void hmc5883l_model_measure(hmc5883l_model_t *m)
{
    m->measure_count++;
    if (!(m->hw.regs[HMC5883L_REG_STATUS] & HMC5883L_STATUS_LOCK)) {
        hmc5883l_model_set_data(m);
        m->hw.regs[HMC5883L_REG_STATUS] |= HMC5883L_STATUS_RDY;
        m->data_read = 0;
    }
    m->gain_code = (uint8_t)(m->hw.regs[HMC5883L_REG_CONFIG_B] >> 5);
}

/* Let simulated time pass */
__attribute__((unused))
static void hmc5883l_model_advance_ms(hmc5883l_model_t *m, uint32_t ms)
{
    uint32_t us = ms * 1000u;
    while (us > 0u && m->next_set_us > 0u) {
        uint32_t step = (us < m->next_set_us) ? us : m->next_set_us;
        us -= step;
        m->next_set_us -= step;
        if (m->next_set_us == 0u) {
            hmc5883l_model_measure(m);
            m->next_set_us = hmc5883l_model_period_us(m);
        }
    }
}

/* HAL delay: ctx is the hw_reg_model_t, as for the I2C callbacks */
__attribute__((unused))
static void hmc5883l_model_delay_ms(void *ctx, uint32_t ms)
{
    hw_model_delay_ms(ctx, ms);
    hmc5883l_model_advance_ms((hmc5883l_model_t *)((hw_reg_model_t *)ctx)->user, ms);
}

/* p14: "After a power-on or a write to the mode or configuration register, the
 * first measurement set is available from all three data output registers
 * after a period of 2/fDO" */
static void hmc5883l_model_restart(hmc5883l_model_t *m)
{
    bool continuous = (m->hw.regs[HMC5883L_REG_MODE] & 0x03u) == 0x00u;
    m->next_set_us = continuous ? 2u * hmc5883l_model_period_us(m) : 0u;
}

/* p16: LOCK ends when "the mode register is changed" or "the measurement
 * configuration (CRA) is changed" */
static void hmc5883l_model_unlock(hmc5883l_model_t *m)
{
    m->hw.regs[HMC5883L_REG_STATUS] &= (uint8_t)~HMC5883L_STATUS_LOCK;
    m->data_read = 0;
}

/* Callback: mode register or CRA write */
static void hmc5883l_model_on_mode_or_cra(void *user, uint8_t reg,
                                          uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    hmc5883l_model_t *m = (hmc5883l_model_t *)user;
    hmc5883l_model_unlock(m);
    hmc5883l_model_restart(m);
}

/* Callback: CRB write */
static void hmc5883l_model_on_crb(void *user, uint8_t reg,
                                  uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    hmc5883l_model_restart((hmc5883l_model_t *)user);
}

/* I2C read with the side effects on Status. ctx is the hw_reg_model_t. */
__attribute__((unused))
static bool hmc5883l_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                     uint8_t *buf, uint16_t len)
{
    hmc5883l_model_t *m = (hmc5883l_model_t *)((hw_reg_model_t *)ctx)->user;
    if (!hw_model_i2c_read_regs(ctx, dev_addr, reg, buf, len)) return false;

    bool mode_read = false;
    for (unsigned i = 0; i < len; i++) {
        unsigned addr = (unsigned)reg + i;
        if (addr >= HMC5883L_REG_DATA_X_H && addr < HMC5883L_REG_STATUS) {
            m->data_read |= (uint8_t)(1u << (addr - HMC5883L_REG_DATA_X_H));
        } else if (addr == HMC5883L_REG_MODE) {
            mode_read = true;
        }
    }

    /* p16: LOCK is set when "some but not all for of the six data output
     * registers have been read" or "Mode register has been read" */
    if (m->data_read == HMC5883L_DATA_ALL_READ) {
        hmc5883l_model_unlock(m);
        if (m->rdy_cleared_by_read) {
            m->hw.regs[HMC5883L_REG_STATUS] &= (uint8_t)~HMC5883L_STATUS_RDY;
        }
    } else if (m->data_read != 0u || mode_read) {
        m->hw.regs[HMC5883L_REG_STATUS] |= HMC5883L_STATUS_LOCK;
    }
    return true;
}

static void hmc5883l_model_init(hmc5883l_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* p12: "CRA default is 0x10", p13: "CRB default is 0x20", p14: "Mode
     * register default is 0x01" */
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_CONFIG_A, 0x10u);
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_CONFIG_B, 0x20u);
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_MODE, 0x01u);
    m->gain_code = 0x01u;

    /* ID registers (read-only): 'H', '4', '3' */
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_ID_A, 'H');
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_ID_B, '4');
    hw_model_set_reset_value(&m->hw, HMC5883L_REG_ID_C, '3');
    hw_model_set_readonly(&m->hw, HMC5883L_REG_ID_A);
    hw_model_set_readonly(&m->hw, HMC5883L_REG_ID_B);
    hw_model_set_readonly(&m->hw, HMC5883L_REG_ID_C);

    /* Data registers 0x03..0x08 (read-only, written by measurement engine) */
    for (unsigned i = 0; i < 6; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(HMC5883L_REG_DATA_X_H + i));
    }

    /* Status is read-only (p16), and p11: "any write to an invalid address
     * location ... is ignored" */
    hw_model_set_readonly(&m->hw, HMC5883L_REG_STATUS);
    for (unsigned i = HMC5883L_REG_ID_C + 1u; i < HW_MODEL_REG_COUNT; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)i);
    }

    /* Config A: only bits [6:0] writable (bit 7 reserved, always 0) */
    hw_model_set_writable_mask(&m->hw, HMC5883L_REG_CONFIG_A, 0x7Fu);

    /* Config B: only bits [7:5] writable (gain) */
    hw_model_set_writable_mask(&m->hw, HMC5883L_REG_CONFIG_B, 0xE0u);

    /* Mode: only bits [1:0] writable */
    hw_model_set_writable_mask(&m->hw, HMC5883L_REG_MODE, 0x03u);

    /* Write callbacks: each of these restarts the wait for the first set */
    hw_model_set_on_write(&m->hw, HMC5883L_REG_MODE, hmc5883l_model_on_mode_or_cra);
    hw_model_set_on_write(&m->hw, HMC5883L_REG_CONFIG_A, hmc5883l_model_on_mode_or_cra);
    hw_model_set_on_write(&m->hw, HMC5883L_REG_CONFIG_B, hmc5883l_model_on_crb);

    /* Default simulated field: ~500 counts each axis at gain 1090 */
    m->true_x = 500;
    m->true_y = -200;
    m->true_z = 350;

    hw_model_reset(&m->hw);
}

#endif /* HW_MODEL_HMC5883L_H */
