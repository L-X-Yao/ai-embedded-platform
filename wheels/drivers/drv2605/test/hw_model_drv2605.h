/*
 * hw_model_drv2605.h — DRV2605L hardware register model
 *
 * Models TI DRV2605L (SLOS854A) register behavior:
 * - STATUS at 0x00: bits [7:5] = device ID (read-only)
 * - MODE at 0x01: operating mode + standby bit
 * - RTP at 0x02: real-time playback value
 * - WAVEFORM at 0x04..0x0B: 8-slot waveform sequencer
 * - GO at 0x0C: bit 0 triggers/stops playback
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
#define DRV2605_REG_WAVEFORM0   0x04
#define DRV2605_REG_GO          0x0C
#define DRV2605_REG_FEEDBACK    0x1A
#define DRV2605_REG_CONTROL3    0x1D

/* Status register bits */
#define DRV2605_STATUS_DEVID_MASK   0xE0
#define DRV2605_DEVID_2605L_VAL     (7u << 5)  /* 0xE0 */

/* Feedback register bits */
#define DRV2605_FEEDBACK_LRA_BIT    0x80

typedef struct {
    hw_reg_model_t  hw;
    int             go_count;       /* number of times GO was set */
    int             stop_count;     /* number of times GO was cleared */
    bool            playing;        /* simulated playback state */
} drv2605_model_t;

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
        m->playing = true;
    }
    if (was_go && !now_go) {
        m->stop_count++;
        m->playing = false;
    }
}

static void drv2605_model_init(drv2605_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Status register: device ID = DRV2605L (7 << 5 = 0xE0), read-only */
    hw_model_set_reset_value(&m->hw, DRV2605_REG_STATUS, DRV2605_DEVID_2605L_VAL);
    hw_model_set_readonly(&m->hw, DRV2605_REG_STATUS);

    /* MODE register: POR default = 0x40 (standby mode) */
    hw_model_set_reset_value(&m->hw, DRV2605_REG_MODE, 0x40);

    /* RTP register: 0x00 at POR */

    /* Waveform slots: 0x00 at POR */

    /* GO register: 0x00 at POR, only bit 0 is writable */
    hw_model_set_writable_mask(&m->hw, DRV2605_REG_GO, 0x01);

    /* FEEDBACK register: POR default 0x36 (ERM, medium brake gain) */
    hw_model_set_reset_value(&m->hw, DRV2605_REG_FEEDBACK, 0x36);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, DRV2605_REG_GO, drv2605_model_on_go);

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

/* HAL delay stub */
__attribute__((unused))
static void drv2605_model_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

#endif /* HW_MODEL_DRV2605_H */
