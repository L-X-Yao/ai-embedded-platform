// SPDX-License-Identifier: MIT
/*
 * hw_model_aw9523.h — AW9523B hardware register model
 *
 * Models Awinic AW9523B (datasheet V1.1.1, May 2016) register behavior:
 * - INPUT_P0/P1 at 0x00/0x01: pin level, input or output (read-only from bus)
 * - OUTPUT_P0/P1 at 0x02/0x03: output latch, POR by AD1/AD0 (Table 1)
 * - CONFIG_P0/P1 at 0x04/0x05: direction (0=output, 1=input), POR=0x00
 * - INT_P0/P1 at 0x06/0x07: interrupt enable (0=enabled, 1=disabled), POR=0x00
 * - ID at 0x10: chip ID (read-only, 0x23)
 * - CTL at 0x11: global control. GPOMD (bit 4) 0 = P0 open-drain, the POR
 *   state, 1 = push-pull. P1 is always push-pull
 * - LED_MODE_P0/P1 at 0x12/0x13: 0=LED, 1=GPIO, POR=0xFF
 * - DIM0-DIM15 at 0x20-0x2F: LED current
 * - Soft reset at 0x7F: only a write of 0x00 triggers full register reset
 * - Only the address set by AD1/AD0 answers, 1 0 1 1 0 AD1 AD0
 *
 * The model simulates the outside world by allowing tests to set pin levels.
 * Input pins, and P0 output pins released in open-drain mode, read that level.
 * Pins driven by the part read what the OUTPUT latch drives.
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

/* CTL bits, Table 10 p19 */
#define AW9523M_CTL_GPOMD       0x10
#define AW9523M_CTL_TEST_BITS   0xEC   /* D[7:5], D[3:2]: "must configure to 0" */

#define AW9523M_ADDR_BASE       0x58   /* 1 0 1 1 0 AD1 AD0 */

typedef struct {
    hw_reg_model_t  hw;
    uint8_t         pin_state_p0;   /* simulated external pin levels for P0 */
    uint8_t         pin_state_p1;   /* simulated external pin levels for P1 */
    uint8_t         addr;           /* I2C address set by AD1/AD0 */
    int             reserved_writes;     /* p20: "user should not write" them */
    int             ctl_test_bit_writes; /* CTL writes with a test bit set */
} aw9523_model_t;

/* Sync INPUT register with the level on each pin (p18: Input shows the pin
 * "regardless of whether the pin is defined as an input or an output"). */
static void aw9523_model_sync_input(aw9523_model_t *m, bool is_p0)
{
    uint8_t config_reg = is_p0 ? AW9523M_REG_CONFIG_P0 : AW9523M_REG_CONFIG_P1;
    uint8_t output_reg = is_p0 ? AW9523M_REG_OUTPUT_P0 : AW9523M_REG_OUTPUT_P1;
    uint8_t input_reg  = is_p0 ? AW9523M_REG_INPUT_P0  : AW9523M_REG_INPUT_P1;
    uint8_t pin_state  = is_p0 ? m->pin_state_p0 : m->pin_state_p1;

    uint8_t config = m->hw.regs[config_reg];  /* 1=input, 0=output */
    uint8_t output = m->hw.regs[output_reg];

    if (is_p0 && !(m->hw.regs[AW9523M_REG_CTL] & AW9523M_CTL_GPOMD)) {
        /* Open-drain: only a 0 in the latch drives. A 1 lets go of the pin,
         * the Hi-Z of Table 1, and the outside decides, as on an input. */
        m->hw.regs[input_reg] = (uint8_t)(pin_state & (config | output));
        return;
    }

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

/* Callback: GPOMD changes how P0 drives its pins */
static void aw9523_model_on_ctl_write(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val; (void)new_val;
    aw9523_model_sync_input((aw9523_model_t *)user, true);
}

/* Callback: soft reset (write 0x00 to 0x7F) restores POR defaults.
 * p16: "Writing 00H to the software register(7FH) will generate a reset pulse." */
static void aw9523_model_on_reset_write(void *user, uint8_t reg,
                                        uint8_t old_val, uint8_t new_val)
{
    (void)reg; (void)old_val;
    aw9523_model_t *m = (aw9523_model_t *)user;
    if (new_val != 0x00) return;
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
    hw_model_set_reset_value(&m->hw, AW9523M_REG_CONFIG_P0, 0x00);   /* all output */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_CONFIG_P1, 0x00);
    hw_model_set_reset_value(&m->hw, AW9523M_REG_INT_P0, 0x00);      /* all enabled */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_INT_P1, 0x00);
    hw_model_set_reset_value(&m->hw, AW9523M_REG_ID, 0x23);          /* chip ID */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_LED_MODE_P0, 0xFF); /* all GPIO mode */
    hw_model_set_reset_value(&m->hw, AW9523M_REG_LED_MODE_P1, 0xFF);

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
    hw_model_set_on_write(&m->hw, AW9523M_REG_CTL, aw9523_model_on_ctl_write);
    hw_model_set_on_write(&m->hw, AW9523M_REG_RESET, aw9523_model_on_reset_write);

    /* Default pin state: all low */
    m->pin_state_p0 = 0x00;
    m->pin_state_p1 = 0x00;

    /* AD1 = AD0 = GND */
    m->addr = AW9523M_ADDR_BASE;

    hw_model_reset(&m->hw);
    aw9523_model_sync_input(m, true);
    aw9523_model_sync_input(m, false);
}

/* Helper: wire AD1 and AD0, true = VBAT, and take the part through POR. Table 1
 * gives the output defaults: AD0 high sets the low nibble of both ports, AD1
 * the high nibble. On P0 that is the Hi-Z of an open-drain 1. */
__attribute__((unused))
static void aw9523_model_set_straps(aw9523_model_t *m, bool ad1, bool ad0)
{
    uint8_t output = (uint8_t)((ad1 ? 0xF0u : 0x00u) | (ad0 ? 0x0Fu : 0x00u));

    m->addr = (uint8_t)(AW9523M_ADDR_BASE | (ad1 ? 2u : 0u) | (ad0 ? 1u : 0u));
    hw_model_set_reset_value(&m->hw, AW9523M_REG_OUTPUT_P0, output);
    hw_model_set_reset_value(&m->hw, AW9523M_REG_OUTPUT_P1, output);
    hw_model_reset(&m->hw);
    aw9523_model_sync_input(m, true);
    aw9523_model_sync_input(m, false);
}

/* p20 Table 14 */
static bool aw9523_model_reserved(uint8_t reg)
{
    return (reg >= 0x08 && reg <= 0x0F) || (reg >= 0x14 && reg <= 0x1F) ||
           (reg >= 0x30 && reg <= 0x7E) || reg >= 0x80;
}

/* I2C HAL: a wrong address gets no ACK */
__attribute__((unused))
static bool aw9523_model_write_reg(void *ctx, uint8_t dev_addr, uint8_t reg,
                                   uint8_t val)
{
    aw9523_model_t *m = (aw9523_model_t *)ctx;
    if (dev_addr != m->addr) return false;
    if (aw9523_model_reserved(reg)) m->reserved_writes++;
    if (reg == AW9523M_REG_CTL && (val & AW9523M_CTL_TEST_BITS)) {
        m->ctl_test_bit_writes++;
    }
    return hw_model_i2c_write_reg(&m->hw, dev_addr, reg, val);
}

__attribute__((unused))
static bool aw9523_model_read_regs(void *ctx, uint8_t dev_addr, uint8_t reg,
                                   uint8_t *buf, uint16_t len)
{
    aw9523_model_t *m = (aw9523_model_t *)ctx;
    if (dev_addr != m->addr) return false;
    return hw_model_i2c_read_regs(&m->hw, dev_addr, reg, buf, len);
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
