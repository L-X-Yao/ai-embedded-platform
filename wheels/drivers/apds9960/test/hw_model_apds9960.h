// SPDX-License-Identifier: MIT
/*
 * hw_model_apds9960.h — APDS-9960 hardware register model from datasheet
 *
 * Models APDS-9960 register behavior (AV02-4191EN):
 * - Reset values and read-only registers from the Register Set, p19 and p20
 * - Chip ID at 0x92: 0xAB (read-only)
 * - Enable register at 0x80 (PON/AEN/PEN/GEN bits)
 * - Data comes from cycles, which tests run with apds9960_model_cycle().
 *   With PON, a cycle fills PDATA and sets PVALID if PEN is on, and fills
 *   RGBC and sets AVALID if AEN is on (p9, p20, p25). Both wait while GEN and
 *   GMODE hold the flow in the gesture engine (p16)
 * - Reading PDATA clears PVALID, reading RGBC clears AVALID, and asserting
 *   PEN or AEN starts the count again (p25)
 * - The gesture FIFO holds 32 datasets, fills only in the gesture engine and
 *   drops new data with GFOV on overflow. Single or page reads step it at
 *   address 0xFF, and it reads zero when empty (p33)
 * - GVALID rises at the GFIFOTH level (p15, p17, p28) and falls when GMODE
 *   is 0 and GFLVL is 0 (p33; p17 mentions emptying alone). The manual
 *   does not say when GFOV falls, so the model drops it with the last dataset
 *
 * Time, thresholds, interrupts, gesture entry by GPENTH and exit by GEXTH
 * are not modelled.
 */

#ifndef HW_MODEL_APDS9960_H
#define HW_MODEL_APDS9960_H

#include "hw_model.h"

/* --- APDS-9960 register addresses --- */
#define APDS_REG_ENABLE   0x80u
#define APDS_REG_ATIME    0x81u
#define APDS_REG_WTIME    0x83u
#define APDS_REG_CONFIG1  0x8Du
#define APDS_REG_PPULSE   0x8Eu
#define APDS_REG_CONTROL  0x8Fu
#define APDS_REG_CONFIG2  0x90u
#define APDS_REG_ID       0x92u
#define APDS_REG_STATUS   0x93u
#define APDS_REG_CDATAL   0x94u
#define APDS_REG_PDATA    0x9Cu
#define APDS_REG_GPENTH   0xA0u
#define APDS_REG_GEXTH    0xA1u
#define APDS_REG_GCONF1   0xA2u
#define APDS_REG_GCONF2   0xA3u
#define APDS_REG_GPULSE   0xA6u
#define APDS_REG_GCONF4   0xABu
#define APDS_REG_GFLVL    0xAEu
#define APDS_REG_GSTATUS  0xAFu
#define APDS_REG_GFIFO_U  0xFCu
#define APDS_REG_GFIFO_R  0xFFu

/* --- register bits --- */
#define APDS_ENABLE_PON     0x01u
#define APDS_ENABLE_AEN     0x02u
#define APDS_ENABLE_PEN     0x04u
#define APDS_ENABLE_GEN     0x40u
#define APDS_STATUS_AVALID  0x01u
#define APDS_STATUS_PVALID  0x02u
#define APDS_GCONF4_GMODE   0x01u
#define APDS_GSTATUS_GVALID 0x01u
#define APDS_GSTATUS_GFOV   0x02u

#define APDS_FIFO_DEPTH     32u

typedef struct {
    hw_reg_model_t  hw;
    uint16_t        true_c, true_r, true_g, true_b;
    uint8_t         true_prox;
    /* gesture FIFO of U/D/L/R datasets; GFLVL holds the level */
    uint8_t         gesture_fifo[APDS_FIFO_DEPTH][4];
    uint8_t         gesture_head;       /* oldest dataset */
} apds9960_model_t;

/* Populate RGBC data registers */
static void apds9960_model_set_color_data(apds9960_model_t *m)
{
    m->hw.regs[0x94] = (uint8_t)(m->true_c & 0xFF);
    m->hw.regs[0x95] = (uint8_t)(m->true_c >> 8);
    m->hw.regs[0x96] = (uint8_t)(m->true_r & 0xFF);
    m->hw.regs[0x97] = (uint8_t)(m->true_r >> 8);
    m->hw.regs[0x98] = (uint8_t)(m->true_g & 0xFF);
    m->hw.regs[0x99] = (uint8_t)(m->true_g >> 8);
    m->hw.regs[0x9A] = (uint8_t)(m->true_b & 0xFF);
    m->hw.regs[0x9B] = (uint8_t)(m->true_b >> 8);
}

static bool apds9960_model_in_gesture_engine(const apds9960_model_t *m)
{
    uint8_t en = m->hw.regs[APDS_REG_ENABLE];
    return (en & APDS_ENABLE_PON) != 0 && (en & APDS_ENABLE_GEN) != 0 &&
           (m->hw.regs[APDS_REG_GCONF4] & APDS_GCONF4_GMODE) != 0;
}

/* One pass of the engines */
__attribute__((unused))
static void apds9960_model_cycle(apds9960_model_t *m)
{
    uint8_t en = m->hw.regs[APDS_REG_ENABLE];

    if ((en & APDS_ENABLE_PON) == 0) return;
    if (apds9960_model_in_gesture_engine(m)) return;

    if (en & APDS_ENABLE_PEN) {
        m->hw.regs[APDS_REG_PDATA] = m->true_prox;
        m->hw.regs[APDS_REG_STATUS] |= APDS_STATUS_PVALID;
    }
    if (en & APDS_ENABLE_AEN) {
        apds9960_model_set_color_data(m);
        m->hw.regs[APDS_REG_STATUS] |= APDS_STATUS_AVALID;
    }
}

/* Callback: enable register write — asserting an engine restarts its count */
static void apds9960_model_on_enable(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    apds9960_model_t *m = (apds9960_model_t *)user;
    uint8_t rising = (uint8_t)(new_val & (uint8_t)~old_val);

    if (rising & APDS_ENABLE_PEN) {
        m->hw.regs[APDS_REG_STATUS] &= (uint8_t)~APDS_STATUS_PVALID;
    }
    if (rising & APDS_ENABLE_AEN) {
        m->hw.regs[APDS_REG_STATUS] &= (uint8_t)~APDS_STATUS_AVALID;
    }
}

static void apds9960_model_fifo_settle(apds9960_model_t *m)
{
    if (m->hw.regs[APDS_REG_GFLVL] != 0) return;
    m->hw.regs[APDS_REG_GSTATUS] &= (uint8_t)~APDS_GSTATUS_GFOV;
    if ((m->hw.regs[APDS_REG_GCONF4] & APDS_GCONF4_GMODE) == 0) {
        m->hw.regs[APDS_REG_GSTATUS] &= (uint8_t)~APDS_GSTATUS_GVALID;
    }
}

/* Callback: GCONF4 write — GMODE going to 0 can end GVALID */
static void apds9960_model_on_gconf4(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    apds9960_model_fifo_settle((apds9960_model_t *)user);
}

static void apds9960_model_push_dataset(apds9960_model_t *m, const uint8_t *set)
{
    static const uint8_t threshold[4] = { 1, 4, 8, 16 };
    uint8_t level = m->hw.regs[APDS_REG_GFLVL];

    if (level == APDS_FIFO_DEPTH) {
        m->hw.regs[APDS_REG_GSTATUS] |= APDS_GSTATUS_GFOV;
        return;
    }
    memcpy(m->gesture_fifo[(m->gesture_head + level) % APDS_FIFO_DEPTH], set, 4);
    level++;
    m->hw.regs[APDS_REG_GFLVL] = level;
    if (level >= threshold[m->hw.regs[APDS_REG_GCONF1] >> 6]) {
        m->hw.regs[APDS_REG_GSTATUS] |= APDS_GSTATUS_GVALID;
    }
}

static void apds9960_model_pop_dataset(apds9960_model_t *m)
{
    if (m->hw.regs[APDS_REG_GFLVL] == 0) return;
    m->gesture_head = (uint8_t)((m->gesture_head + 1u) % APDS_FIFO_DEPTH);
    m->hw.regs[APDS_REG_GFLVL]--;
    apds9960_model_fifo_settle(m);
}

/* Custom read callback: FIFO reads step the FIFO, data reads clear VALID */
static bool apds9960_model_i2c_read_regs(void *ctx, uint8_t dev_addr,
                                          uint8_t reg, uint8_t *buf,
                                          uint16_t len)
{
    (void)dev_addr;
    apds9960_model_t *m = (apds9960_model_t *)ctx;
    uint8_t addr = reg;

    if (hw_model__should_fail(&m->hw)) return false;
    for (uint16_t i = 0; i < len; i++) {
        if (addr >= APDS_REG_GFIFO_U) {
            uint8_t col = (uint8_t)(addr - APDS_REG_GFIFO_U);
            buf[i] = m->hw.regs[APDS_REG_GFLVL] != 0
                         ? m->gesture_fifo[m->gesture_head][col] : 0u;
            if (addr == APDS_REG_GFIFO_R) {
                apds9960_model_pop_dataset(m);
                addr = APDS_REG_GFIFO_U;
            } else {
                addr++;
            }
            continue;
        }
        buf[i] = m->hw.regs[addr];
        if (addr == APDS_REG_PDATA) {
            m->hw.regs[APDS_REG_STATUS] &= (uint8_t)~APDS_STATUS_PVALID;
        } else if (addr >= APDS_REG_CDATAL && addr < APDS_REG_PDATA) {
            m->hw.regs[APDS_REG_STATUS] &= (uint8_t)~APDS_STATUS_AVALID;
        }
        addr++;
    }
    return true;
}

static void apds9960_model_init(apds9960_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID: 0xAB at 0x92 (read-only) */
    hw_model_set_reset_value(&m->hw, APDS_REG_ID, 0xABu);
    hw_model_set_readonly(&m->hw, APDS_REG_ID);

    /* The other non-zero reset values, p19 and p20 */
    hw_model_set_reset_value(&m->hw, APDS_REG_ATIME, 0xFFu);
    hw_model_set_reset_value(&m->hw, APDS_REG_WTIME, 0xFFu);
    hw_model_set_reset_value(&m->hw, APDS_REG_CONFIG1, 0x40u);
    hw_model_set_reset_value(&m->hw, APDS_REG_PPULSE, 0x40u);
    hw_model_set_reset_value(&m->hw, APDS_REG_CONFIG2, 0x01u);
    hw_model_set_reset_value(&m->hw, APDS_REG_GPULSE, 0x40u);

    /* Status register (read-only) */
    hw_model_set_readonly(&m->hw, APDS_REG_STATUS);

    /* RGBC data registers (read-only) */
    for (unsigned i = 0; i < 8; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(APDS_REG_CDATAL + i));
    }

    /* Proximity data (read-only) */
    hw_model_set_readonly(&m->hw, APDS_REG_PDATA);

    /* Gesture FIFO level and status (read-only) */
    hw_model_set_readonly(&m->hw, APDS_REG_GFLVL);
    hw_model_set_readonly(&m->hw, APDS_REG_GSTATUS);

    /* Gesture FIFO (read-only) */
    for (unsigned i = 0; i < 4; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(APDS_REG_GFIFO_U + i));
    }

    /* Bit 7 of ENABLE and bits 7:2 of GCONF4 are reserved */
    hw_model_set_writable_mask(&m->hw, APDS_REG_ENABLE, 0x7Fu);
    hw_model_set_writable_mask(&m->hw, APDS_REG_GCONF4, 0x03u);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, APDS_REG_ENABLE, apds9960_model_on_enable);
    hw_model_set_on_write(&m->hw, APDS_REG_GCONF4, apds9960_model_on_gconf4);

    /* Default simulated color/proximity */
    m->true_c = 1200;
    m->true_r = 400;
    m->true_g = 500;
    m->true_b = 300;
    m->true_prox = 128;

    hw_model_reset(&m->hw);
}

/* Helper: the gesture engine produces count datasets of 4 bytes each */
static void apds9960_model_set_gesture(apds9960_model_t *m,
                                        uint8_t count,
                                        const uint8_t *data)
{
    if (!apds9960_model_in_gesture_engine(m)) return;
    for (uint8_t i = 0; i < count; i++) {
        apds9960_model_push_dataset(m, data + (unsigned)i * 4u);
    }
}

#endif /* HW_MODEL_APDS9960_H */
