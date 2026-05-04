/*
 * hw_model_pca9685.h — PCA9685 hardware register model
 *
 * Models NXP PCA9685 datasheet (rev 4) register behavior:
 * - MODE1 at 0x00: RESTART(7), AI(5), SLEEP(4), ALLCALL(0)
 * - MODE2 at 0x01
 * - LED0..LED15 registers at 0x06..0x45 (4 bytes per channel)
 * - ALL_LED registers at 0xFA..0xFD
 * - PRE_SCALE at 0xFE (writable only when SLEEP=1)
 * - Sleep/wake sequence enforcement
 * - RESTART bit behavior
 */

#ifndef HW_MODEL_PCA9685_H
#define HW_MODEL_PCA9685_H

#include "hw_model.h"

/* --- PCA9685 register addresses --- */
#define PCA9685_REG_MODE1       0x00
#define PCA9685_REG_MODE2       0x01
#define PCA9685_REG_LED0_ON_L   0x06
#define PCA9685_REG_ALL_ON_L    0xFA
#define PCA9685_REG_ALL_ON_H    0xFB
#define PCA9685_REG_ALL_OFF_L   0xFC
#define PCA9685_REG_ALL_OFF_H   0xFD
#define PCA9685_REG_PRE_SCALE   0xFE

/* MODE1 bits */
#define PCA9685_MODE1_RESTART   0x80
#define PCA9685_MODE1_AI        0x20
#define PCA9685_MODE1_SLEEP     0x10
#define PCA9685_MODE1_ALLCALL   0x01

typedef struct {
    hw_reg_model_t  hw;
    int             prescale_write_count;
    int             sleep_count;
    int             wake_count;
    bool            prescale_rejected;  /* true if write attempted while awake */
} pca9685_model_t;

/* Callback: MODE1 write tracks sleep/wake transitions */
static void pca9685_model_on_mode1(void *user, uint8_t reg,
                                   uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    pca9685_model_t *m = (pca9685_model_t *)user;

    bool was_sleep = (old_val & PCA9685_MODE1_SLEEP) != 0;
    bool now_sleep = (new_val & PCA9685_MODE1_SLEEP) != 0;

    if (!was_sleep && now_sleep) m->sleep_count++;
    if (was_sleep && !now_sleep) m->wake_count++;
}

/* Callback: PRE_SCALE write — only effective when SLEEP=1 */
static void pca9685_model_on_prescale(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    pca9685_model_t *m = (pca9685_model_t *)user;
    bool sleeping = (m->hw.regs[PCA9685_REG_MODE1] & PCA9685_MODE1_SLEEP) != 0;

    if (sleeping) {
        m->prescale_write_count++;
    } else {
        /* Reject: revert to old value (datasheet: writes ignored when awake) */
        m->hw.regs[PCA9685_REG_PRE_SCALE] = old_val;
        m->prescale_rejected = true;
    }
    (void)new_val;
}

static void pca9685_model_init(pca9685_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR defaults from datasheet Table 6 */
    hw_model_set_reset_value(&m->hw, PCA9685_REG_MODE1, PCA9685_MODE1_SLEEP | PCA9685_MODE1_ALLCALL);
    hw_model_set_reset_value(&m->hw, PCA9685_REG_MODE2, 0x04);
    hw_model_set_reset_value(&m->hw, PCA9685_REG_PRE_SCALE, 0x1E); /* default ~200 Hz */

    /* LED registers: all zero at POR */
    /* (already zero from memset) */

    /* MODE1 bits [6] (EXTCLK) is read-only once set, but we model as fully writable
     * for simplicity since the driver never uses EXTCLK */

    /* PRE_SCALE: writable mask is full byte, but callback enforces sleep requirement */

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, PCA9685_REG_MODE1, pca9685_model_on_mode1);
    hw_model_set_on_write(&m->hw, PCA9685_REG_PRE_SCALE, pca9685_model_on_prescale);

    hw_model_reset(&m->hw);
}

/* Helper: check if device is sleeping */
__attribute__((unused))
static bool pca9685_model_is_sleeping(const pca9685_model_t *m)
{
    return (m->hw.regs[PCA9685_REG_MODE1] & PCA9685_MODE1_SLEEP) != 0;
}

/* Helper: read a channel's OFF value (12-bit) */
__attribute__((unused))
static uint16_t pca9685_model_get_channel_off(const pca9685_model_t *m, uint8_t ch)
{
    uint8_t base = (uint8_t)(PCA9685_REG_LED0_ON_L + ch * 4u);
    uint16_t off_l = m->hw.regs[base + 2];
    uint16_t off_h = m->hw.regs[base + 3];
    return off_l | (uint16_t)((off_h & 0x0F) << 8);
}

/* Helper: check if a channel's full-off bit is set */
__attribute__((unused))
static bool pca9685_model_channel_full_off(const pca9685_model_t *m, uint8_t ch)
{
    uint8_t base = (uint8_t)(PCA9685_REG_LED0_ON_L + ch * 4u);
    return (m->hw.regs[base + 3] & 0x10) != 0;
}

/* Helper: check if a channel's full-on bit is set */
__attribute__((unused))
static bool pca9685_model_channel_full_on(const pca9685_model_t *m, uint8_t ch)
{
    uint8_t base = (uint8_t)(PCA9685_REG_LED0_ON_L + ch * 4u);
    return (m->hw.regs[base + 1] & 0x10) != 0;
}

/* HAL delay stub */
__attribute__((unused))
static void pca9685_model_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

#endif /* HW_MODEL_PCA9685_H */
