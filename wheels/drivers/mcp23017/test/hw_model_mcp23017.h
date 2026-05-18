// SPDX-License-Identifier: MIT
/*
 * hw_model_mcp23017.h — MCP23017 hardware register model
 *
 * Models Microchip MCP23017 (DS20001952C) Bank=0 register behavior:
 * - IODIRA/B at 0x00/0x01: direction (1=input, 0=output), POR=0xFF
 * - IPOLA/B at 0x02/0x03: input polarity
 * - GPINTENA/B at 0x04/0x05: interrupt-on-change enable
 * - DEFVALA/B at 0x06/0x07: default compare value
 * - INTCONA/B at 0x08/0x09: interrupt control
 * - IOCON at 0x0A/0x0B: configuration (shared between A/B)
 * - GPPUA/B at 0x0C/0x0D: pull-up enable
 * - INTFA/B at 0x0E/0x0F: interrupt flags (read-only, set by model)
 * - INTCAPA/B at 0x10/0x11: interrupt capture (read-only)
 * - GPIOA/B at 0x12/0x13: port value (reads pin state)
 * - OLATA/B at 0x14/0x15: output latch
 *
 * The model simulates pin input by allowing tests to set GPIO read values.
 * INTF and INTCAP are read-only from the bus perspective.
 */

#ifndef HW_MODEL_MCP23017_H
#define HW_MODEL_MCP23017_H

#include "hw_model.h"

/* --- Bank=0 register addresses --- */
#define MCP23017_REG_IODIRA     0x00
#define MCP23017_REG_IODIRB     0x01
#define MCP23017_REG_IPOLA      0x02
#define MCP23017_REG_IPOLB      0x03
#define MCP23017_REG_GPINTENA   0x04
#define MCP23017_REG_GPINTENB   0x05
#define MCP23017_REG_DEFVALA    0x06
#define MCP23017_REG_DEFVALB    0x07
#define MCP23017_REG_INTCONA    0x08
#define MCP23017_REG_INTCONB    0x09
#define MCP23017_REG_IOCON_A    0x0A
#define MCP23017_REG_IOCON_B    0x0B
#define MCP23017_REG_GPPUA      0x0C
#define MCP23017_REG_GPPUB      0x0D
#define MCP23017_REG_INTFA      0x0E
#define MCP23017_REG_INTFB      0x0F
#define MCP23017_REG_INTCAPA    0x10
#define MCP23017_REG_INTCAPB    0x11
#define MCP23017_REG_GPIOA      0x12
#define MCP23017_REG_GPIOB      0x13
#define MCP23017_REG_OLATA      0x14
#define MCP23017_REG_OLATB      0x15

typedef struct {
    hw_reg_model_t  hw;
    uint8_t         pin_state_a;    /* simulated external pin levels for port A */
    uint8_t         pin_state_b;    /* simulated external pin levels for port B */
} mcp23017_model_t;

/* Callback: GPIO register write — output pins drive the "pin state",
 * input pins are ignored (driven by external simulation). */
static void mcp23017_model_on_gpio_write(void *user, uint8_t reg,
                                         uint8_t old_val, uint8_t new_val)
{
    (void)old_val;
    mcp23017_model_t *m = (mcp23017_model_t *)user;

    /* Determine which port */
    bool is_a = (reg == MCP23017_REG_GPIOA || reg == MCP23017_REG_OLATA);
    uint8_t iodir = is_a ? m->hw.regs[MCP23017_REG_IODIRA]
                         : m->hw.regs[MCP23017_REG_IODIRB];
    uint8_t *pin_state = is_a ? &m->pin_state_a : &m->pin_state_b;

    /* For output pins (IODIR=0), update pin state to match written value.
     * For input pins (IODIR=1), pin state stays as externally set. */
    uint8_t output_mask = (uint8_t)~iodir;
    *pin_state = (uint8_t)((*pin_state & iodir) | (new_val & output_mask));

    /* Also sync OLAT register to track output latch state */
    uint8_t olat_reg = is_a ? MCP23017_REG_OLATA : MCP23017_REG_OLATB;
    m->hw.regs[olat_reg] = (uint8_t)((m->hw.regs[olat_reg] & iodir) | (new_val & output_mask));
}

/* Callback: OLAT write drives output pins */
static void mcp23017_model_on_olat_write(void *user, uint8_t reg,
                                         uint8_t old_val, uint8_t new_val)
{
    /* OLAT write is equivalent to GPIO write for output pins */
    mcp23017_model_on_gpio_write(user, reg, old_val, new_val);
}

/* Callback: IODIR write updates what GPIO reads return */
static void mcp23017_model_on_iodir_write(void *user, uint8_t reg,
                                          uint8_t old_val, uint8_t new_val)
{
    (void)old_val; (void)new_val;
    mcp23017_model_t *m = (mcp23017_model_t *)user;

    /* Sync GPIO register: input pins read from pin_state, output from OLAT */
    bool is_a = (reg == MCP23017_REG_IODIRA);
    uint8_t iodir   = new_val;
    uint8_t pin     = is_a ? m->pin_state_a : m->pin_state_b;
    uint8_t olat    = is_a ? m->hw.regs[MCP23017_REG_OLATA] : m->hw.regs[MCP23017_REG_OLATB];
    uint8_t gpio_reg = is_a ? MCP23017_REG_GPIOA : MCP23017_REG_GPIOB;

    m->hw.regs[gpio_reg] = (uint8_t)((pin & iodir) | (olat & ~iodir));
}

static void mcp23017_model_init(mcp23017_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR defaults: IODIR=0xFF (all input), all others=0x00 */
    hw_model_set_reset_value(&m->hw, MCP23017_REG_IODIRA, 0xFF);
    hw_model_set_reset_value(&m->hw, MCP23017_REG_IODIRB, 0xFF);
    /* All other registers default to 0x00 */

    /* INTF registers are read-only (set by interrupt hardware) */
    hw_model_set_readonly(&m->hw, MCP23017_REG_INTFA);
    hw_model_set_readonly(&m->hw, MCP23017_REG_INTFB);

    /* INTCAP registers are read-only (captured on interrupt) */
    hw_model_set_readonly(&m->hw, MCP23017_REG_INTCAPA);
    hw_model_set_readonly(&m->hw, MCP23017_REG_INTCAPB);

    /* Write callbacks */
    hw_model_set_on_write(&m->hw, MCP23017_REG_GPIOA, mcp23017_model_on_gpio_write);
    hw_model_set_on_write(&m->hw, MCP23017_REG_GPIOB, mcp23017_model_on_gpio_write);
    hw_model_set_on_write(&m->hw, MCP23017_REG_OLATA, mcp23017_model_on_olat_write);
    hw_model_set_on_write(&m->hw, MCP23017_REG_OLATB, mcp23017_model_on_olat_write);
    hw_model_set_on_write(&m->hw, MCP23017_REG_IODIRA, mcp23017_model_on_iodir_write);
    hw_model_set_on_write(&m->hw, MCP23017_REG_IODIRB, mcp23017_model_on_iodir_write);

    /* Default pin state: all low */
    m->pin_state_a = 0x00;
    m->pin_state_b = 0x00;

    hw_model_reset(&m->hw);
}

/* Helper: simulate external pin input on port A */
__attribute__((unused))
static void mcp23017_model_set_pin_input_a(mcp23017_model_t *m, uint8_t value)
{
    m->pin_state_a = value;
    /* Update GPIO register for input pins */
    uint8_t iodir = m->hw.regs[MCP23017_REG_IODIRA];
    m->hw.regs[MCP23017_REG_GPIOA] =
        (uint8_t)((value & iodir) | (m->hw.regs[MCP23017_REG_OLATA] & ~iodir));
}

/* Helper: simulate external pin input on port B */
__attribute__((unused))
static void mcp23017_model_set_pin_input_b(mcp23017_model_t *m, uint8_t value)
{
    m->pin_state_b = value;
    uint8_t iodir = m->hw.regs[MCP23017_REG_IODIRB];
    m->hw.regs[MCP23017_REG_GPIOB] =
        (uint8_t)((value & iodir) | (m->hw.regs[MCP23017_REG_OLATB] & ~iodir));
}

/* Helper: simulate interrupt flag */
__attribute__((unused))
static void mcp23017_model_set_intf(mcp23017_model_t *m, bool port_a, uint8_t flags)
{
    uint8_t reg = port_a ? MCP23017_REG_INTFA : MCP23017_REG_INTFB;
    m->hw.regs[reg] = flags;  /* bypass read-only for simulation */
}

/* HAL delay stub */
__attribute__((unused))
static void mcp23017_model_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

#endif /* HW_MODEL_MCP23017_H */
