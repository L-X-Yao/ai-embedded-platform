// SPDX-License-Identifier: MIT
/*
 * hw_model_cst816s.h — datasheet-accurate register model for CST816S
 *
 * Models CST816S register behavior:
 * - Chip ID 0xB5 at 0xA7 (read-only)
 * - Firmware version at 0xA9 (read-only)
 * - Touch data at 0x01..0x06 (read-only, set by inject helper)
 * - MotionMask at 0xEC (writable)
 * - DisAutoSleep at 0xFE (writable)
 * - AutoSleepTime at 0xF9 (writable, 2 s after reset)
 */

#ifndef HW_MODEL_CST816S_H
#define HW_MODEL_CST816S_H

#include "hw_model.h"

/* --- CST816S register addresses --- */
#define CST816S_REG_GESTURE_ID      0x01u
#define CST816S_REG_FINGER_NUM      0x02u
#define CST816S_REG_XH              0x03u
#define CST816S_REG_XL              0x04u
#define CST816S_REG_YH              0x05u
#define CST816S_REG_YL              0x06u
#define CST816S_REG_CHIP_ID         0xA7u
#define CST816S_REG_FW_VERSION      0xA9u
#define CST816S_REG_MOTION_MASK     0xECu
#define CST816S_REG_AUTO_SLEEP_TIME 0xF9u
#define CST816S_REG_DIS_AUTO_SLEEP  0xFEu

/* --- Simulated physical environment --- */
typedef struct {
    hw_reg_model_t hw;
    uint16_t       touch_x;
    uint16_t       touch_y;
    uint8_t        touch_event;    /* 0=down, 1=up, 2=contact */
    uint8_t        touch_gesture;
    uint8_t        touch_fingers;
} cst816s_model_t;

/* Write touch data into registers (called when touch state changes) */
static void cst816s_model_update_touch_regs(cst816s_model_t *m)
{
    m->hw.regs[CST816S_REG_GESTURE_ID] = m->touch_gesture;
    m->hw.regs[CST816S_REG_FINGER_NUM] = m->touch_fingers;
    m->hw.regs[CST816S_REG_XH] = (uint8_t)((m->touch_event << 6) |
                                             ((m->touch_x >> 8) & 0x0Fu));
    m->hw.regs[CST816S_REG_XL] = (uint8_t)(m->touch_x & 0xFFu);
    m->hw.regs[CST816S_REG_YH] = (uint8_t)((m->touch_y >> 8) & 0x0Fu);
    m->hw.regs[CST816S_REG_YL] = (uint8_t)(m->touch_y & 0xFFu);
}

static void cst816s_model_init(cst816s_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* Chip ID (read-only, 0xB5 for CST816S) */
    hw_model_set_reset_value(&m->hw, CST816S_REG_CHIP_ID, 0xB5);
    hw_model_set_readonly(&m->hw, CST816S_REG_CHIP_ID);

    /* Firmware version (read-only) */
    hw_model_set_reset_value(&m->hw, CST816S_REG_FW_VERSION, 0x03);
    hw_model_set_readonly(&m->hw, CST816S_REG_FW_VERSION);

    /* Touch data registers (read-only, written by touch engine) */
    hw_model_set_readonly(&m->hw, CST816S_REG_GESTURE_ID);
    hw_model_set_readonly(&m->hw, CST816S_REG_FINGER_NUM);
    hw_model_set_readonly(&m->hw, CST816S_REG_XH);
    hw_model_set_readonly(&m->hw, CST816S_REG_XL);
    hw_model_set_readonly(&m->hw, CST816S_REG_YH);
    hw_model_set_readonly(&m->hw, CST816S_REG_YL);

    /* Writable config registers keep defaults (all bits writable) */
    /* MotionMask (0xEC) default 0x00 — no gestures enabled */
    /* DisAutoSleep (0xFE) default 0x00 — auto sleep enabled */
    /* AutoSleepTime (0xF9) default 2 s */
    hw_model_set_reset_value(&m->hw, CST816S_REG_AUTO_SLEEP_TIME, 0x02);

    hw_model_reset(&m->hw);
}

/* Helper: inject touch data into the model */
static void cst816s_model_inject_touch(cst816s_model_t *m,
                                       uint16_t x, uint16_t y,
                                       uint8_t event, uint8_t gesture,
                                       uint8_t fingers)
{
    m->touch_x       = x;
    m->touch_y       = y;
    m->touch_event   = event;
    m->touch_gesture = gesture;
    m->touch_fingers = fingers;
    cst816s_model_update_touch_regs(m);
}

/* Helper: clear touch (no finger) */
static void cst816s_model_clear_touch(cst816s_model_t *m)
{
    cst816s_model_inject_touch(m, 0, 0, 0, 0, 0);
}

#endif /* HW_MODEL_CST816S_H */
