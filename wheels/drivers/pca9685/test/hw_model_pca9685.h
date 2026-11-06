// SPDX-License-Identifier: MIT
/*
 * hw_model_pca9685.h — PCA9685 hardware register model
 *
 * Models NXP PCA9685 datasheet (rev 4) register behavior:
 * - MODE1 at 0x00: RESTART(7), EXTCLK(6), AI(5), SLEEP(4), ALLCALL(0)
 * - MODE2 at 0x01, SUBADR1-3 and ALLCALLADR at 0x02..0x05
 * - LED0..LED15 registers at 0x06..0x45 (4 bytes per channel)
 * - ALL_LED registers at 0xFA..0xFD: fill every channel, read zero
 * - PRE_SCALE at 0xFE (writable only when SLEEP=1, minimum 3)
 * - Reset values, read-only bits, reserved addresses that are not acknowledged
 * - RESTART as Section 7.3.1.1 gives it: only the chip sets it, at the end of
 *   the refresh cycle after a sleep with a channel active, and a written 1
 *   clears it. Channels resume only through that 1, written with SLEEP at 0
 *   for 500 us.
 *
 * Time passes only through pca9685_model_advance_us() and the HAL delay.
 */

#ifndef HW_MODEL_PCA9685_H
#define HW_MODEL_PCA9685_H

#include "hw_model.h"

/* --- PCA9685 register addresses --- */
#define PCA9685_REG_MODE1       0x00
#define PCA9685_REG_MODE2       0x01
#define PCA9685_REG_SUBADR1     0x02
#define PCA9685_REG_SUBADR2     0x03
#define PCA9685_REG_SUBADR3     0x04
#define PCA9685_REG_ALLCALLADR  0x05
#define PCA9685_REG_LED0_ON_L   0x06
#define PCA9685_REG_LED15_OFF_H 0x45
#define PCA9685_REG_ALL_ON_L    0xFA
#define PCA9685_REG_ALL_ON_H    0xFB
#define PCA9685_REG_ALL_OFF_L   0xFC
#define PCA9685_REG_ALL_OFF_H   0xFD
#define PCA9685_REG_PRE_SCALE   0xFE

/* MODE1 bits */
#define PCA9685_MODE1_RESTART   0x80
#define PCA9685_MODE1_EXTCLK    0x40
#define PCA9685_MODE1_AI        0x20
#define PCA9685_MODE1_SLEEP     0x10
#define PCA9685_MODE1_ALLCALL   0x01

#define PCA9685_MODEL_CHANNELS  16u
#define PCA9685_MODEL_FULL_BIT  0x10   /* bit 4 of LEDn_ON_H and LEDn_OFF_H */
#define PCA9685_MODEL_HIGH_MASK 0x1F   /* bits 7:5 are "non-writable" */

/* p.14: "It takes 500 us max. for the oscillator to be up and running" */
#define PCA9685_MODEL_OSC_START_US 500u

typedef struct {
    hw_reg_model_t  hw;
    int             prescale_write_count;
    int             sleep_count;
    int             wake_count;
    bool            prescale_rejected;  /* true if write attempted while awake */
    uint32_t        refresh_left_us;    /* to the end of the refresh cycle */
    uint32_t        osc_start_left_us;  /* to a running oscillator */
    bool            channels_stopped;   /* by sleep, until a restart */
    bool            restart_too_soon;   /* a 1 written inside the 500 us */
} pca9685_model_t;

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
    return (uint16_t)(off_l | (uint16_t)((off_h & 0x0F) << 8));
}

/* Helper: check if a channel's full-off bit is set */
__attribute__((unused))
static bool pca9685_model_channel_full_off(const pca9685_model_t *m, uint8_t ch)
{
    uint8_t base = (uint8_t)(PCA9685_REG_LED0_ON_L + ch * 4u);
    return (m->hw.regs[base + 3] & PCA9685_MODEL_FULL_BIT) != 0;
}

/* Helper: check if a channel is full on. p.24: with both bits set, "the
 * LEDn_OFF_H[4] function takes precedence." */
__attribute__((unused))
static bool pca9685_model_channel_full_on(const pca9685_model_t *m, uint8_t ch)
{
    uint8_t base = (uint8_t)(PCA9685_REG_LED0_ON_L + ch * 4u);
    return (m->hw.regs[base + 1] & PCA9685_MODEL_FULL_BIT) != 0
           && !pca9685_model_channel_full_off(m, ch);
}

/* Channels resume after a sleep only through a restart. p.14: "No PWM control
 * is possible when the oscillator is off." */
__attribute__((unused))
static bool pca9685_model_channels_running(const pca9685_model_t *m)
{
    return !pca9685_model_is_sleeping(m) && !m->channels_stopped;
}

__attribute__((unused))
static bool pca9685_model_restart_too_soon(const pca9685_model_t *m)
{
    return m->restart_too_soon;
}

/* p.15 footnote: an orderly shutdown sets full OFF on every active channel. */
static bool pca9685_model_any_channel_active(const pca9685_model_t *m)
{
    for (uint8_t ch = 0; ch < PCA9685_MODEL_CHANNELS; ch++)
        if (!pca9685_model_channel_full_off(m, ch))
            return true;
    return false;
}

/* One PWM cycle: 4096 * (prescale + 1) periods of the 25 MHz oscillator. */
static uint32_t pca9685_model_refresh_us(const pca9685_model_t *m)
{
    return ((uint32_t)m->hw.regs[PCA9685_REG_PRE_SCALE] + 1u) * 4096u / 25u;
}

/* Lets time pass. A sleep may fall anywhere in a cycle: the model takes the
 * longest wait, a full cycle. */
__attribute__((unused))
static void pca9685_model_advance_us(pca9685_model_t *m, uint32_t us)
{
    m->osc_start_left_us =
        (us < m->osc_start_left_us) ? m->osc_start_left_us - us : 0u;

    if (m->refresh_left_us == 0)
        return;
    if (us < m->refresh_left_us) {
        m->refresh_left_us -= us;
        return;
    }
    m->refresh_left_us = 0;
    m->hw.regs[PCA9685_REG_MODE1] =
        (uint8_t)(m->hw.regs[PCA9685_REG_MODE1] | PCA9685_MODE1_RESTART);
    m->channels_stopped = true;
}

/* Callback: MODE1 write tracks sleep/wake transitions and the RESTART logic */
static void pca9685_model_on_mode1(void *user, uint8_t reg,
                                   uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    pca9685_model_t *m = (pca9685_model_t *)user;

    bool was_sleep = (old_val & PCA9685_MODE1_SLEEP) != 0;
    bool now_sleep = (new_val & PCA9685_MODE1_SLEEP) != 0;
    bool restart_enabled = (old_val & PCA9685_MODE1_RESTART) != 0;
    bool restart_written = (new_val & PCA9685_MODE1_RESTART) != 0;

    if (!was_sleep && now_sleep) {
        m->sleep_count++;
        if (pca9685_model_any_channel_active(m))
            m->refresh_left_us = pca9685_model_refresh_us(m);
    }
    if (was_sleep && !now_sleep) {
        m->wake_count++;
        m->osc_start_left_us = PCA9685_MODEL_OSC_START_US;
        /* The manual is silent on a wake inside the cycle: nothing has
         * stopped yet, so the channels run on. */
        m->refresh_left_us = 0;
    }

    /* p.14: "User writes logic 1 to this bit to clear it to logic 0. A user
     * write of logic 0 will have no effect." EXTCLK: "a 'sticky bit', that
     * is, it cannot be cleared by writing a logic 0 to it." */
    uint8_t stored = (uint8_t)(new_val & ~PCA9685_MODE1_RESTART);
    if (restart_enabled && !restart_written)
        stored = (uint8_t)(stored | PCA9685_MODE1_RESTART);
    stored = (uint8_t)(stored | (old_val & PCA9685_MODE1_EXTCLK));
    m->hw.regs[PCA9685_REG_MODE1] = stored;

    if (!restart_written)
        return;
    /* p.15: "The SLEEP bit must be logic 0 for at least 500 us, before a
     * logic 1 is written into the RESTART bit." */
    if (was_sleep || now_sleep || m->osc_start_left_us != 0)
        m->restart_too_soon = true;
    else if (restart_enabled)
        m->channels_stopped = false;
}

/* Callback: PRE_SCALE write — only effective when SLEEP=1 */
static void pca9685_model_on_prescale(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    pca9685_model_t *m = (pca9685_model_t *)user;
    bool sleeping = (m->hw.regs[PCA9685_REG_MODE1] & PCA9685_MODE1_SLEEP) != 0;

    if (sleeping) {
        m->prescale_write_count++;
        /* p.25: "The hardware forces a minimum value" of 3 */
        if (new_val < 3u)
            m->hw.regs[PCA9685_REG_PRE_SCALE] = 3;
    } else {
        /* Reject: revert to old value (datasheet: writes ignored when awake) */
        m->hw.regs[PCA9685_REG_PRE_SCALE] = old_val;
        m->prescale_rejected = true;
    }
}

/* Callback: PWM register write. p.15: with OCH at 0, a write to any of them
 * then a STOP clears RESTART, and every HAL write ends in a STOP. OCH at 1 is
 * not modelled: the driver leaves it at 0. */
static void pca9685_model_on_led(void *user, uint8_t reg,
                                 uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    pca9685_model_t *m = (pca9685_model_t *)user;
    m->hw.regs[PCA9685_REG_MODE1] =
        (uint8_t)(m->hw.regs[PCA9685_REG_MODE1] & ~PCA9685_MODE1_RESTART);
}

/* Callback: ALL_LED write fills that register of every channel, and Table 4
 * gives these registers as "write/read zero". */
static void pca9685_model_on_all_led(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)old_val;
    pca9685_model_t *m = (pca9685_model_t *)user;
    unsigned offset = (unsigned)reg - PCA9685_REG_ALL_ON_L;

    for (unsigned ch = 0; ch < PCA9685_MODEL_CHANNELS; ch++)
        m->hw.regs[PCA9685_REG_LED0_ON_L + ch * 4u + offset] = new_val;
    m->hw.regs[reg] = 0;
}

static void pca9685_model_init(pca9685_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR defaults: Table 5 p.14, Table 6 p.16, Tables 8 to 10 p.25 and p.26 */
    hw_model_set_reset_value(&m->hw, PCA9685_REG_MODE1, PCA9685_MODE1_SLEEP | PCA9685_MODE1_ALLCALL);
    hw_model_set_reset_value(&m->hw, PCA9685_REG_MODE2, 0x04);
    hw_model_set_reset_value(&m->hw, PCA9685_REG_SUBADR1, 0xE2);
    hw_model_set_reset_value(&m->hw, PCA9685_REG_SUBADR2, 0xE4);
    hw_model_set_reset_value(&m->hw, PCA9685_REG_SUBADR3, 0xE8);
    hw_model_set_reset_value(&m->hw, PCA9685_REG_ALLCALLADR, 0xE0);
    hw_model_set_reset_value(&m->hw, PCA9685_REG_PRE_SCALE, 0x1E); /* default ~200 Hz */

    /* Table 6: MODE2 bits 7 to 5 are read only. p.26: so is the LSB of the
     * address registers. */
    hw_model_set_writable_mask(&m->hw, PCA9685_REG_MODE2, 0x1F);
    hw_model_set_writable_mask(&m->hw, PCA9685_REG_SUBADR1, 0xFE);
    hw_model_set_writable_mask(&m->hw, PCA9685_REG_SUBADR2, 0xFE);
    hw_model_set_writable_mask(&m->hw, PCA9685_REG_SUBADR3, 0xFE);
    hw_model_set_writable_mask(&m->hw, PCA9685_REG_ALLCALLADR, 0xFE);

    /* Table 7: every channel resets to full OFF, with bits 7:5 of the high
     * bytes non-writable */
    for (unsigned ch = 0; ch < PCA9685_MODEL_CHANNELS; ch++) {
        uint8_t base = (uint8_t)(PCA9685_REG_LED0_ON_L + ch * 4u);
        hw_model_set_reset_value(&m->hw, (uint8_t)(base + 3u), PCA9685_MODEL_FULL_BIT);
        hw_model_set_writable_mask(&m->hw, (uint8_t)(base + 1u), PCA9685_MODEL_HIGH_MASK);
        hw_model_set_writable_mask(&m->hw, (uint8_t)(base + 3u), PCA9685_MODEL_HIGH_MASK);
    }
    hw_model_set_writable_mask(&m->hw, PCA9685_REG_ALL_ON_H, PCA9685_MODEL_HIGH_MASK);
    hw_model_set_writable_mask(&m->hw, PCA9685_REG_ALL_OFF_H, PCA9685_MODEL_HIGH_MASK);

    /* PRE_SCALE: writable mask is full byte, but callback enforces sleep requirement */

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, PCA9685_REG_MODE1, pca9685_model_on_mode1);
    hw_model_set_on_write(&m->hw, PCA9685_REG_PRE_SCALE, pca9685_model_on_prescale);
    for (unsigned reg = PCA9685_REG_LED0_ON_L; reg <= PCA9685_REG_LED15_OFF_H; reg++)
        hw_model_set_on_write(&m->hw, (uint8_t)reg, pca9685_model_on_led);
    for (unsigned reg = PCA9685_REG_ALL_ON_L; reg <= PCA9685_REG_ALL_OFF_H; reg++)
        hw_model_set_on_write(&m->hw, (uint8_t)reg, pca9685_model_on_all_led);

    hw_model_reset(&m->hw);
}

/* HAL write. p.13: "All further addresses are reserved for future use;
 * reserved addresses will not be acknowledged." */
__attribute__((unused))
static bool pca9685_model_write_reg(void *ctx, uint8_t dev_addr,
                                    uint8_t reg, uint8_t val)
{
    if (reg > PCA9685_REG_LED15_OFF_H && reg < PCA9685_REG_ALL_ON_L)
        return false;
    return hw_model_i2c_write_reg(ctx, dev_addr, reg, val);
}

/* HAL delay: the time the driver waits is the time the chip sees. */
__attribute__((unused))
static void pca9685_model_delay(void *ctx, uint32_t ms)
{
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    hw_model_delay_ms(hw, ms);
    pca9685_model_advance_us((pca9685_model_t *)hw->user, ms * 1000u);
}

#endif /* HW_MODEL_PCA9685_H */
