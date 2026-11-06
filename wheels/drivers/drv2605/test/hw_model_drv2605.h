// SPDX-License-Identifier: MIT
/*
 * hw_model_drv2605.h — DRV2605L hardware register model
 *
 * Models TI DRV2605L (SLOS854D) register behavior:
 * - Reset values of all registers from Table 3, p.35
 * - STATUS at 0x00: bits [7:5] = device ID (read-only)
 * - MODE at 0x01: DEV_RESET, STANDBY, MODE[2:0]. DEV_RESET is a power cycle and
 *   clears itself, STANDBY stops any playback at once
 * - RTP at 0x02: real-time playback value, driven only in ready RTP mode
 * - WAVEFORM at 0x04..0x0B: 8-slot waveform sequencer
 * - GO at 0x0C: bit 0 starts playback in ready internal trigger mode. It stays
 *   high until drv2605_model_end_playback() stands for the end of the sequence,
 *   and clearing it before then cancels. With nothing queued it clears at once
 * - FEEDBACK at 0x1A: bit 7 = ERM/LRA select
 * - CONTROL3 at 0x1D
 *
 * Device ID: DRV2605=3, DRV2605L=7 (bits [7:5] of status).
 */

#ifndef HW_MODEL_DRV2605_H
#define HW_MODEL_DRV2605_H

#include "hw_model.h"

/* --- Register addresses --- */
#define DRV2605_REG_STATUS      0x00
#define DRV2605_REG_MODE        0x01
#define DRV2605_REG_RTP         0x02
#define DRV2605_REG_LIBRARY     0x03
#define DRV2605_REG_WAVEFORM0   0x04
#define DRV2605_REG_GO          0x0C
#define DRV2605_REG_FEEDBACK    0x1A
#define DRV2605_REG_CONTROL3    0x1D

/* Status register bits */
#define DRV2605_STATUS_DEVID_MASK   0xE0
#define DRV2605_DEVID_2605L_VAL     (7u << 5)  /* 0xE0 */

/* Mode register bits */
#define DRV2605_MODE_DEV_RESET_BIT  0x80u
#define DRV2605_MODE_STANDBY_BIT    0x40u
#define DRV2605_MODE_FIELD          0x07u

/* Feedback register bits */
#define DRV2605_FEEDBACK_LRA_BIT    0x80

/* Table 3, p.35: the defaults of 0x00 to 0x22 */
static const uint8_t drv2605_model_defaults[0x23] = {
    0xE0, 0x40, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,  /* 0x00 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 0x08 */
    0x00, 0x05, 0x19, 0xFF, 0x19, 0xFF, 0x3E, 0x8C,  /* 0x10 */
    0x0C, 0x6C, 0x36, 0x93, 0xF5, 0xA0, 0x20, 0x80,  /* 0x18 */
    0x33, 0x00, 0x00,                                /* 0x20 */
};

typedef struct {
    hw_reg_model_t  hw;
    int             go_count;       /* number of times GO was set */
    int             stop_count;     /* number of times GO was cleared by a write */
    bool            playing;        /* simulated playback state */
} drv2605_model_t;

/* p.37: GO fires waveforms in internal trigger mode. p.21: not in standby. */
static bool drv2605_model_can_play(const drv2605_model_t *m)
{
    uint8_t mode = m->hw.regs[DRV2605_REG_MODE];

    return !(mode & DRV2605_MODE_STANDBY_BIT) &&
           (mode & DRV2605_MODE_FIELD) == 0;
}

/* Callback: GO register triggers/stops playback */
static void drv2605_model_on_go(void *user, uint8_t reg,
                                uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    drv2605_model_t *m = (drv2605_model_t *)user;
    bool was_go = (old_val & 0x01) != 0;
    bool now_go = (new_val & 0x01) != 0;

    if (!was_go && now_go) {
        m->go_count++;
        if (drv2605_model_can_play(m)) {
            /* p.39: the sequencer stops at an identifier of zero */
            if (m->hw.regs[DRV2605_REG_WAVEFORM0] != 0) {
                m->playing = true;
            } else {
                m->hw.regs[DRV2605_REG_GO] = 0x00;
            }
        }
    }
    if (was_go && !now_go) {
        /* p.22: "Clearing the GO bit during waveform playback cancels the
         * waveform sequence." */
        m->stop_count++;
        m->playing = false;
    }
}

/* Callback: MODE register resets and enters standby */
static void drv2605_model_on_mode(void *user, uint8_t reg,
                                  uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    drv2605_model_t *m = (drv2605_model_t *)user;

    /* p.37: "Any playback operations are immediately interrupted, and all
     * registers are reset to the default values." */
    if (new_val & DRV2605_MODE_DEV_RESET_BIT) {
        m->playing = false;
        hw_model_reset(&m->hw);
        return;
    }

    /* p.21: "if a waveform is played, it immediately stops when the STANDBY bit
     * is asserted" */
    if ((new_val & DRV2605_MODE_STANDBY_BIT) && m->playing) {
        m->playing = false;
        m->hw.regs[DRV2605_REG_GO] = 0x00;
    }
}

static void drv2605_model_init(drv2605_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    for (uint8_t reg = 0; reg < sizeof(drv2605_model_defaults); reg++) {
        hw_model_set_reset_value(&m->hw, reg, drv2605_model_defaults[reg]);
    }

    /* Status register: device ID = DRV2605L (7 << 5 = 0xE0), read-only */
    hw_model_set_readonly(&m->hw, DRV2605_REG_STATUS);

    /* MODE: DEV_RESET, STANDBY and MODE[2:0], bits 5:3 are reserved */
    hw_model_set_writable_mask(&m->hw, DRV2605_REG_MODE, 0xC7);

    /* Library: HI_Z and LIBRARY_SEL[2:0], the rest is reserved */
    hw_model_set_writable_mask(&m->hw, DRV2605_REG_LIBRARY, 0x17);

    /* GO register: 0x00 at POR, only bit 0 is writable */
    hw_model_set_writable_mask(&m->hw, DRV2605_REG_GO, 0x01);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, DRV2605_REG_GO, drv2605_model_on_go);
    hw_model_set_on_write(&m->hw, DRV2605_REG_MODE, drv2605_model_on_mode);

    hw_model_reset(&m->hw);
}

/* Helper: set device ID (for testing wrong-ID rejection) */
__attribute__((unused))
static void drv2605_model_set_devid(drv2605_model_t *m, uint8_t devid_3bit)
{
    uint8_t status = (uint8_t)(devid_3bit << 5);
    m->hw.regs[DRV2605_REG_STATUS] = status;
    m->hw.reset_values[DRV2605_REG_STATUS] = status;
}

/* Helper: the sequence has played out. p.22: "The GO bit remains high until the
 * playback of the haptic waveform sequence is complete." */
__attribute__((unused))
static void drv2605_model_end_playback(drv2605_model_t *m)
{
    m->playing = false;
    m->hw.regs[DRV2605_REG_GO] = 0x00;
}

/* Helper: p.38, the engine drives RTP_INPUT "when MODE[2:0] = 5 (RTP mode)" */
__attribute__((unused))
static bool drv2605_model_rtp_driving(const drv2605_model_t *m)
{
    uint8_t mode = m->hw.regs[DRV2605_REG_MODE];

    return !(mode & DRV2605_MODE_STANDBY_BIT) &&
           (mode & DRV2605_MODE_FIELD) == 5;
}

/* HAL delay stub */
__attribute__((unused))
static void drv2605_model_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

#endif /* HW_MODEL_DRV2605_H */
