/*
 * hw_model_aw9523.h — AW9523B hardware register model
 *
 * Models Awinic AW9523B (datasheet rev 1.5) register behavior:
 * - INPUT_P0/P1 at 0x00/0x01: pin input state (read-only from bus)
 * - OUTPUT_P0/P1 at 0x02/0x03: output latch
 * - CONFIG_P0/P1 at 0x04/0x05: direction (0=output, 1=input), POR=0xFF
 * - INT_P0/P1 at 0x06/0x07: interrupt enable (0=enabled, 1=disabled), POR=0xFF
 * - ID at 0x10: chip ID (read-only, 0x23)
 * - CTL at 0x11: global control
 * - LED_MODE_P0/P1 at 0x12/0x13: 0=LED, 1=GPIO, POR=0x00
 * - DIM0-DIM15 at 0x20-0x2F: LED current
 * - Soft reset at 0x7F: write 0x00 triggers full register reset
 *
 * The model simulates pin input by allowing tests to set INPUT values.
 * When pins are configured as output, INPUT reflects the OUTPUT latch.
 */

#ifndef HW_MODEL_AW9523_H
#define HW_MODEL_AW9523_H

#include "hw_model.h"

/* --- Register addresses --- */
#define AW9523M_REG_INPUT_P0    0x00
#define AW9523M_REG_INPUT_P1    0x01
#define AW9523M_REG_OUTPUT_P0   0x02
#define AW9523M_REG_OUTPUT_P1   0x03
#define AW9523M_REG_CONFIG_P0   0x04
#define AW9523M_REG_CONFIG_P1   0x05
#define AW9523M_REG_INT_P0      0x06
#define AW9523M_REG_INT_P1      0x07
#define AW9523M_REG_ID          0x10
#define AW9523M_REG_CTL         0x11
#define AW9523M_REG_LED_MODE_P0 0x12
#define AW9523M_REG_LED_MODE_P1 0x13
#define AW9523M_REG_DIM_BASE    0x20
#define AW9523M_REG_RESET       0x7F

typedef struct {
    hw_reg_model_t  hw;
    uint8_t         pin_state_p0;   /* simulated external pin levels for P0 */
    uint8_t         pin_state_p1;   /* simulated external pin levels for P1 */
} aw9523_model_t;

/* Sync INPUT register: output pins reflect OUTPUT latch, input pins
 * reflect externally simulated pin_state. */
static void aw9523_model_sync_input(aw9523_model_t *m, bool is_p0)
{
    uint8_t config_reg = is_p0 ? AW9523M_REG_CONFIG_P0 : AW9523M_REG_CONFIG_P1;
    uint8_t output_reg = is_p0 ? AW9523M_REG_OUTPUT_P0 : AW9523M_REG_OUTPUT_P1;
    uint8_t input_reg  = is_p0 ? AW9523M_REG_INPUT_P0  : AW9523M_REG_INPUT_P1;
    uint8_t pin_state  = is_p0 ? m->pin_state_p0 : m->pin_state_p1;

    uint8_t config = m->hw.regs[config_reg];  /* 1=input, 0=output */
    uint8_t output = m->hw.regs[output_reg];

    /* Input pins read from external pin_state, output pins read from output latch */
    m->hw.regs[input_reg] = (uint8_t)((pin_state & config) | (output & ~config));
}

/* Callback: OUTPUT write updates INPUT register for output pins */
static void aw9523_model_on_output_write(void *user, uint8_t reg,
                                         uint8_t old_val, uint8_t new_val)
{
    (void)old_val; (void)new_val;
    aw9523_model_t *m = (aw9523_model_t *)user;
    bool is_p0 = (reg == AW9523M_REG_OUTPUT_P0);
    aw9523_model_sync_input(m, is_p0);
}

/* Callback: CONFIG (direction) write updates INPUT register */
static void aw9523_model_on_config_write(void *user, uint8_t reg,
                                         uint8_t old_val, uint8_t new_val)
{
    (void)old_val; (void)new_val;
    aw9523_model_t *m = (aw9523_model_t *)user;
    bool is_p0 = (reg == AW9523M_REG_CONFIG_P0);
    aw9523_model_sync_input(m, is_p0);
}

/* Callback: soft reset (write 0x00 to 0x7F) restores POR defaults */
static void aw9523_model_on_reset_write(void *user, uint8_t reg,
                                        uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    aw9523_model_t *m = (aw9523_model_t *)user;
    hw_model_reset(&m->hw);
    aw9523_model_sync_input(m, true);
    aw9523_model_sync_input(m, false);
}

static void aw9523_model_init(aw9523_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR defaults */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_CONFIG_P0, 0xFF);   /* all input */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_CONFIG_P1, 0xFF);
    hw_model_set_reset_value(&m->hw, AW9523M_REG_INT_P0, 0xFF);      /* all disabled */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_INT_P1, 0xFF);
    hw_model_set_reset_value(&m->hw, AW9523M_REG_ID, 0x23);          /* chip ID */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_LED_MODE_P0, 0x00); /* all LED mode */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_LED_MODE_P1, 0x00);

    /* INPUT registers are read-only from bus */
    hw_model_set_readonly(&m->hw, AW9523M_REG_INPUT_P0);
    hw_model_set_readonly(&m->hw, AW9523M_REG_INPUT_P1);

    /* ID register is read-only */
    hw_model_set_readonly(&m->hw, AW9523M_REG_ID);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, AW9523M_REG_OUTPUT_P0, aw9523_model_on_output_write);
    hw_model_set_on_write(&m->hw, AW9523M_REG_OUTPUT_P1, aw9523_model_on_output_write);
    hw_model_set_on_write(&m->hw, AW9523M_REG_CONFIG_P0, aw9523_model_on_config_write);
    hw_model_set_on_write(&m->hw, AW9523M_REG_CONFIG_P1, aw9523_model_on_config_write);
    hw_model_set_on_write(&m->hw, AW9523M_REG_RESET, aw9523_model_on_reset_write);

    /* Default pin state: all low */
    m->pin_state_p0 = 0x00;
    m->pin_state_p1 = 0x00;

    hw_model_reset(&m->hw);
}

/* Helper: simulate external pin input on P0 */
__attribute__((unused))
static void aw9523_model_set_pin_input_p0(aw9523_model_t *m, uint8_t value)
{
    m->pin_state_p0 = value;
    aw9523_model_sync_input(m, true);
}

/* Helper: simulate external pin input on P1 */
__attribute__((unused))
static void aw9523_model_set_pin_input_p1(aw9523_model_t *m, uint8_t value)
{
    m->pin_state_p1 = value;
    aw9523_model_sync_input(m, false);
}

/* HAL delay stub */
__attribute__((unused))
static void aw9523_model_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

#endif /* HW_MODEL_AW9523_H */
