// SPDX-License-Identifier: MIT
/*
 * hw_model_lis3mdl.h — LIS3MDL hardware register model from datasheet
 *
 * Follows ST DocID024204 Rev 6:
 * - Register map and reset values, Table 16 on p23: the chip powers up with
 *   CTRL_REG1 = 0x10 and CTRL_REG3 = 0x03, which is power-down
 * - The SUB MSb decides whether the address steps (p17): without it every
 *   byte of a read comes from the same register
 * - Writes to reserved or unlisted registers, which "may cause permanent
 *   damage" (p23), are dropped and counted, as are zero bits set in
 *   CTRL_REG2 to CTRL_REG5 (p25 to p27)
 * - CTRL_REG2 SOFT_RST restores the defaults, REBOOT reloads memory content
 *   and leaves the configuration (p23, p25)
 * - In continuous mode a sample comes every 1/ODR, the first one a period
 *   after the mode starts (Table 20 on p24, Table 22 on p25, LP on p26).
 *   Time passes only through the delay callback
 * - STATUS_REG (p28): a sample sets ZYXDA and the axis flags, and one that
 *   overwrites an unread set adds the overrun flags. The manual does not
 *   say which read clears a flag, so the model takes the axis's high byte
 * - BDU holds an unread sample (p27), BLE swaps the output bytes (p26)
 *
 * Single-conversion mode, temperature and interrupts are not modelled.
 */

#ifndef HW_MODEL_LIS3MDL_H
#define HW_MODEL_LIS3MDL_H

#include "hw_model.h"

/* --- LIS3MDL register addresses --- */
#define LIS3MDL_REG_WHO_AM_I   0x0Fu
#define LIS3MDL_REG_CTRL_REG1  0x20u
#define LIS3MDL_REG_CTRL_REG2  0x21u
#define LIS3MDL_REG_CTRL_REG3  0x22u
#define LIS3MDL_REG_CTRL_REG4  0x23u
#define LIS3MDL_REG_CTRL_REG5  0x24u
#define LIS3MDL_REG_STATUS     0x27u
#define LIS3MDL_REG_OUT_X_L    0x28u
#define LIS3MDL_REG_OUT_X_H    0x29u
#define LIS3MDL_REG_OUT_Y_L    0x2Au
#define LIS3MDL_REG_OUT_Y_H    0x2Bu
#define LIS3MDL_REG_OUT_Z_L    0x2Cu
#define LIS3MDL_REG_OUT_Z_H    0x2Du
#define LIS3MDL_REG_TEMP_OUT_L 0x2Eu
#define LIS3MDL_REG_TEMP_OUT_H 0x2Fu
#define LIS3MDL_REG_INT_CFG    0x30u
#define LIS3MDL_REG_INT_SRC    0x31u
#define LIS3MDL_REG_INT_THS_L  0x32u
#define LIS3MDL_REG_INT_THS_H  0x33u

/* --- register bits --- */
#define LIS3MDL_CTRL2_REBOOT   0x08u
#define LIS3MDL_CTRL2_SOFT_RST 0x04u
#define LIS3MDL_CTRL3_LP       0x20u
#define LIS3MDL_CTRL3_MD_MASK  0x03u
#define LIS3MDL_CTRL4_BLE      0x02u
#define LIS3MDL_CTRL5_BDU      0x40u
#define LIS3MDL_STATUS_ZYXOR   0x80u
#define LIS3MDL_STATUS_OR_AXES 0x70u
#define LIS3MDL_STATUS_ZYXDA   0x08u
#define LIS3MDL_STATUS_DA_AXES 0x07u

typedef struct {
    hw_reg_model_t  hw;
    int16_t         true_x;     /* simulated raw X counts */
    int16_t         true_y;     /* simulated raw Y counts */
    int16_t         true_z;     /* simulated raw Z counts */
    int             measure_count;
    uint32_t        sample_age_us;  /* since the last sample */
    int             reboot_count;
    int             reserved_writes;
} lis3mdl_model_t;

static void lis3mdl_model_put(lis3mdl_model_t *m, uint8_t reg, int16_t val)
{
    bool ble = (m->hw.regs[LIS3MDL_REG_CTRL_REG4] & LIS3MDL_CTRL4_BLE) != 0;
    uint8_t lo = (uint8_t)((uint16_t)val & 0xFFu);
    uint8_t hi = (uint8_t)((uint16_t)val >> 8);
    m->hw.regs[reg] = ble ? hi : lo;
    m->hw.regs[reg + 1u] = ble ? lo : hi;
}

/* Populate data registers from true values */
static void lis3mdl_model_set_data(lis3mdl_model_t *m)
{
    lis3mdl_model_put(m, LIS3MDL_REG_OUT_X_L, m->true_x);
    lis3mdl_model_put(m, LIS3MDL_REG_OUT_Y_L, m->true_y);
    lis3mdl_model_put(m, LIS3MDL_REG_OUT_Z_L, m->true_z);
}

static void lis3mdl_model_sample(lis3mdl_model_t *m)
{
    uint8_t status = m->hw.regs[LIS3MDL_REG_STATUS];
    bool unread = (status & LIS3MDL_STATUS_ZYXDA) != 0;
    bool held = unread &&
                (m->hw.regs[LIS3MDL_REG_CTRL_REG5] & LIS3MDL_CTRL5_BDU) != 0;

    m->measure_count++;
    if (held) return;
    lis3mdl_model_set_data(m);
    if (unread) status |= LIS3MDL_STATUS_ZYXOR | LIS3MDL_STATUS_OR_AXES;
    status |= LIS3MDL_STATUS_ZYXDA | LIS3MDL_STATUS_DA_AXES;
    m->hw.regs[LIS3MDL_REG_STATUS] = status;
}

/* Reading an axis's high byte takes its flags, the set's fall with the last */
static void lis3mdl_model_on_read(lis3mdl_model_t *m, uint8_t reg)
{
    uint8_t status = m->hw.regs[LIS3MDL_REG_STATUS];
    uint8_t axis;

    if (reg == LIS3MDL_REG_OUT_X_H) axis = 0x01u;
    else if (reg == LIS3MDL_REG_OUT_Y_H) axis = 0x02u;
    else if (reg == LIS3MDL_REG_OUT_Z_H) axis = 0x04u;
    else return;

    status = (uint8_t)(status & (uint8_t)~(axis | (uint8_t)(axis << 4)));
    if ((status & LIS3MDL_STATUS_DA_AXES) == 0) {
        status = (uint8_t)(status & (uint8_t)~LIS3MDL_STATUS_ZYXDA);
    }
    if ((status & LIS3MDL_STATUS_OR_AXES) == 0) {
        status = (uint8_t)(status & (uint8_t)~LIS3MDL_STATUS_ZYXOR);
    }
    m->hw.regs[LIS3MDL_REG_STATUS] = status;
}

/* One sample period */
static uint32_t lis3mdl_model_period_us(const lis3mdl_model_t *m)
{
    static const uint32_t odr_us[8] = {
        1600000u, 800000u, 400000u, 200000u, 100000u, 50000u, 25000u, 12500u,
    };
    /* FAST_ODR by OM: LP 1000 Hz, MP 560 Hz, HP 300 Hz, UHP 155 Hz */
    static const uint32_t fast_us[4] = { 1000u, 1786u, 3334u, 6452u };
    uint8_t ctrl1 = m->hw.regs[LIS3MDL_REG_CTRL_REG1];

    if (m->hw.regs[LIS3MDL_REG_CTRL_REG3] & LIS3MDL_CTRL3_LP) return odr_us[0];
    if (ctrl1 & 0x02u) return fast_us[(ctrl1 >> 5) & 0x03u];
    return odr_us[(ctrl1 >> 2) & 0x07u];
}

/* Callback: CTRL_REG2 write — the two command bits */
static void lis3mdl_model_on_ctrl2(void *user, uint8_t reg,
                                   uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    lis3mdl_model_t *m = (lis3mdl_model_t *)user;
    if (new_val & LIS3MDL_CTRL2_SOFT_RST) {
        hw_model_reset(&m->hw);
        m->sample_age_us = 0;
    } else if (new_val & LIS3MDL_CTRL2_REBOOT) {
        /* The manual does not say when the bit falls, the model drops it */
        m->reboot_count++;
        m->hw.regs[LIS3MDL_REG_CTRL_REG2] =
            (uint8_t)(new_val & (uint8_t)~LIS3MDL_CTRL2_REBOOT);
    }
}

/* Callback: CTRL_REG3 write — continuous mode (MD = 00) starts the clock */
static void lis3mdl_model_on_ctrl3(void *user, uint8_t reg,
                                    uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    lis3mdl_model_t *m = (lis3mdl_model_t *)user;
    if ((new_val & LIS3MDL_CTRL3_MD_MASK) == 0 &&
        (old_val & LIS3MDL_CTRL3_MD_MASK) != 0) {
        m->sample_age_us = 0;
    }
}

/* Time passes: samples fall due in continuous mode */
__attribute__((unused))
static void lis3mdl_model_delay_ms(void *ctx, uint32_t ms)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    lis3mdl_model_t *m = (lis3mdl_model_t *)hw->user;

    hw->delay_ms_total += ms;
    if (hw->regs[LIS3MDL_REG_CTRL_REG3] & LIS3MDL_CTRL3_MD_MASK) return;

    uint32_t period = lis3mdl_model_period_us(m);
    m->sample_age_us += ms * 1000u;
    while (m->sample_age_us >= period) {
        m->sample_age_us -= period;
        lis3mdl_model_sample(m);
    }
}

static bool lis3mdl_model_reg_exists(uint8_t reg)
{
    return reg == LIS3MDL_REG_WHO_AM_I ||
           (reg >= LIS3MDL_REG_CTRL_REG1 && reg <= LIS3MDL_REG_CTRL_REG5) ||
           (reg >= LIS3MDL_REG_STATUS && reg <= LIS3MDL_REG_INT_THS_H);
}

__attribute__((unused))
static bool lis3mdl_hw_model_write_reg(void *ctx, uint8_t dev_addr,
                                       uint8_t reg, uint8_t val)
{
    (void)dev_addr;
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    lis3mdl_model_t *m = (lis3mdl_model_t *)hw->user;
    uint8_t base = reg & 0x7Fu;

    if (hw_model__should_fail(hw)) return false;
    if (!lis3mdl_model_reg_exists(base)) {
        m->reserved_writes++;
        return true;
    }
    if (base >= LIS3MDL_REG_CTRL_REG2 && base <= LIS3MDL_REG_CTRL_REG5 &&
        (val & (uint8_t)~hw->writable_mask[base]) != 0) {
        m->reserved_writes++;
    }
    hw_model__write_reg(hw, base, val);
    return true;
}

/*
 * Replaces hw_model_i2c_read_regs: with the SUB MSb set the address steps,
 * without it every byte comes from the same register.
 */
__attribute__((unused))
static bool lis3mdl_hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                        uint8_t reg, uint8_t *buf,
                                        uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    lis3mdl_model_t *m = (lis3mdl_model_t *)hw->user;
    uint8_t addr = reg & 0x7Fu;
    bool step = (reg & 0x80u) != 0;

    if (hw_model__should_fail(hw)) return false;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = hw->regs[addr];
        lis3mdl_model_on_read(m, addr);
        if (step) addr = (uint8_t)((addr + 1u) & 0x7Fu);
    }
    return true;
}

static void lis3mdl_model_init(lis3mdl_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Defaults, Table 16 on p23 */
    hw_model_set_reset_value(&m->hw, LIS3MDL_REG_WHO_AM_I, 0x3Du);
    hw_model_set_reset_value(&m->hw, LIS3MDL_REG_CTRL_REG1, 0x10u);
    hw_model_set_reset_value(&m->hw, LIS3MDL_REG_CTRL_REG3, 0x03u);
    hw_model_set_reset_value(&m->hw, LIS3MDL_REG_INT_CFG, 0xE8u);

    /* Type r in Table 16 */
    hw_model_set_readonly(&m->hw, LIS3MDL_REG_WHO_AM_I);
    hw_model_set_readonly(&m->hw, LIS3MDL_REG_STATUS);
    for (unsigned i = 0; i < 8; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(LIS3MDL_REG_OUT_X_L + i));
    }
    hw_model_set_readonly(&m->hw, LIS3MDL_REG_INT_SRC);

    /* 0 FS1 FS0 0 REBOOT SOFT_RST 0 0, Table 23 on p25 */
    hw_model_set_writable_mask(&m->hw, LIS3MDL_REG_CTRL_REG2, 0x6Cu);
    /* 0 0 LP 0 0 SIM MD1 MD0, Table 26 on p26 */
    hw_model_set_writable_mask(&m->hw, LIS3MDL_REG_CTRL_REG3, 0x27u);
    /* 0 0 0 0 OMZ1 OMZ0 BLE 0, Table 29 on p26 */
    hw_model_set_writable_mask(&m->hw, LIS3MDL_REG_CTRL_REG4, 0x0Eu);
    /* FAST_READ BDU 0 0 0 0 0 0, Table 32 on p27 */
    hw_model_set_writable_mask(&m->hw, LIS3MDL_REG_CTRL_REG5, 0xC0u);
    /* XIEN YIEN ZIEN 0 1 IEA LIR IEN, Table 36 on p29 */
    hw_model_set_writable_mask(&m->hw, LIS3MDL_REG_INT_CFG, 0xE7u);

    hw_model_set_on_write(&m->hw, LIS3MDL_REG_CTRL_REG2, lis3mdl_model_on_ctrl2);
    hw_model_set_on_write(&m->hw, LIS3MDL_REG_CTRL_REG3, lis3mdl_model_on_ctrl3);

    /* Default simulated field: moderate values */
    m->true_x = 1000;
    m->true_y = -500;
    m->true_z = 750;

    hw_model_reset(&m->hw);
}

/*
 * Inject magnetic field in microtesla for a given range. Computes the raw
 * count that the driver should read back as ~ut, once a sample takes it.
 */
static void lis3mdl_model_inject_ut(lis3mdl_model_t *m,
                                     float x_ut, float y_ut, float z_ut,
                                     float sensitivity)
{
    m->true_x = (int16_t)(x_ut / 100.0f * sensitivity);
    m->true_y = (int16_t)(y_ut / 100.0f * sensitivity);
    m->true_z = (int16_t)(z_ut / 100.0f * sensitivity);
}

#endif /* HW_MODEL_LIS3MDL_H */
