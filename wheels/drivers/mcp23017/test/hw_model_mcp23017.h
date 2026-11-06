// SPDX-License-Identifier: MIT
/*
 * hw_model_mcp23017.h — MCP23017 hardware register model
 *
 * Models Microchip MCP23017 (DS20001952C) Bank=0 register behavior:
 * - IODIRA/B at 0x00/0x01: direction (1=input, 0=output), POR=0xFF
 * - IPOLA/B at 0x02/0x03: GPIO shows inputs inverted (p18)
 * - GPINTENA/B, DEFVALA/B, INTCONA/B at 0x04-0x09: interrupt-on-change
 * - IOCON at 0x0A and 0x0B: one register at two addresses (Table 3-1, p12),
 *   bit 0 unimplemented (p21)
 * - GPPUA/B at 0x0C/0x0D: pull-up enable, stored only
 * - INTFA/B at 0x0E/0x0F, INTCAPA/B at 0x10/0x11: read-only, filled by the
 *   interrupt logic
 * - GPIOA/B at 0x12/0x13: a read gives the port, a write goes to the whole
 *   latch (p23)
 * - OLATA/B at 0x14/0x15: a read gives the latch, a write drives the output
 *   pins (p24)
 *
 * Interrupt logic, p24 and p25: only enabled inputs count. The first event
 * fills INTF and INTCAP and blocks the port until a read of GPIO or INTCAP.
 * Pin-change mode then takes the level at that read as the new default, and
 * DEFVAL mode raises it again while the pin differs. MIRROR ORs the two
 * pins. Polarity, open drain and BANK=1 addressing are not modelled.
 *
 * Tests change input pins through the set_pin_input helpers.
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

#define MCP23017_IOCON_MIRROR   0x40
#define MCP23017_IOCON_SEQOP    0x20
#define MCP23017_IOCON_MASK     0xFE

/* Bank=0 interleaves the ports: port 0 is A, port 1 is B */
#define MCP23017_PORT_OF(reg)   ((unsigned)(reg) & 1u)

typedef struct {
    hw_reg_model_t  hw;
    uint8_t         pin_state_a;    /* simulated external pin levels for port A */
    uint8_t         pin_state_b;    /* simulated external pin levels for port B */
    uint8_t         ioc_ref[2];     /* levels pin-change mode compares against */
    bool            int_pending[2];
} mcp23017_model_t;

static uint8_t *mcp23017_model_pins(mcp23017_model_t *m, unsigned port)
{
    return port == 0 ? &m->pin_state_a : &m->pin_state_b;
}

/* Outputs follow the latch, and GPIO shows the port */
static void mcp23017_model_refresh(mcp23017_model_t *m, unsigned port)
{
    uint8_t *regs = m->hw.regs;
    uint8_t *pins = mcp23017_model_pins(m, port);
    uint8_t iodir = regs[MCP23017_REG_IODIRA + port];
    uint8_t olat = regs[MCP23017_REG_OLATA + port];
    uint8_t latched = (uint8_t)(olat & (uint8_t)~iodir);

    *pins = (uint8_t)((*pins & iodir) | latched);
    regs[MCP23017_REG_GPIOA + port] = (uint8_t)
        (((*pins ^ regs[MCP23017_REG_IPOLA + port]) & iodir) | latched);
}

static void mcp23017_model_check_ioc(mcp23017_model_t *m, unsigned port)
{
    uint8_t *regs = m->hw.regs;
    uint8_t pins = *mcp23017_model_pins(m, port);
    uint8_t enabled = (uint8_t)(regs[MCP23017_REG_GPINTENA + port] &
                                regs[MCP23017_REG_IODIRA + port]);
    uint8_t intcon = regs[MCP23017_REG_INTCONA + port];
    uint8_t ref = (uint8_t)((m->ioc_ref[port] & (uint8_t)~intcon) |
                            (regs[MCP23017_REG_DEFVALA + port] & intcon));
    uint8_t hit = (uint8_t)((pins ^ ref) & enabled);

    if (m->int_pending[port]) return;

    /* A pin that cannot interrupt keeps no old level to compare with */
    m->ioc_ref[port] = (uint8_t)((m->ioc_ref[port] & enabled) |
                                 (pins & (uint8_t)~enabled));
    if (hit == 0) return;
    regs[MCP23017_REG_INTFA + port] = hit;
    regs[MCP23017_REG_INTCAPA + port] = regs[MCP23017_REG_GPIOA + port];
    m->int_pending[port] = true;
}

/* A read of GPIO or INTCAP clears the interrupt */
static void mcp23017_model_clear_int(mcp23017_model_t *m, unsigned port)
{
    if (!m->int_pending[port]) return;
    m->int_pending[port] = false;
    m->hw.regs[MCP23017_REG_INTFA + port] = 0;
    m->ioc_ref[port] = *mcp23017_model_pins(m, port);
    mcp23017_model_check_ioc(m, port);
}

/* Callback: a GPIO write goes to the whole latch */
static void mcp23017_model_on_gpio_write(void *user, uint8_t reg,
                                         uint8_t old_val, uint8_t new_val)
{
    (void)old_val;
    mcp23017_model_t *m = (mcp23017_model_t *)user;
    unsigned port = MCP23017_PORT_OF(reg);

    m->hw.regs[MCP23017_REG_OLATA + port] = new_val;
    mcp23017_model_refresh(m, port);
}

/* Callback: OLAT, IODIR and IPOL all change what the port shows */
static void mcp23017_model_on_port_write(void *user, uint8_t reg,
                                         uint8_t old_val, uint8_t new_val)
{
    (void)old_val; (void)new_val;
    mcp23017_model_t *m = (mcp23017_model_t *)user;
    unsigned port = MCP23017_PORT_OF(reg);

    mcp23017_model_refresh(m, port);
    mcp23017_model_check_ioc(m, port);
}

/* Callback: GPINTEN, DEFVAL and INTCON change what counts as an event */
static void mcp23017_model_on_ioc_write(void *user, uint8_t reg,
                                        uint8_t old_val, uint8_t new_val)
{
    (void)old_val; (void)new_val;
    mcp23017_model_check_ioc((mcp23017_model_t *)user, MCP23017_PORT_OF(reg));
}

/* Callback: 0Ah and 0Bh are one register */
static void mcp23017_model_on_iocon_write(void *user, uint8_t reg,
                                          uint8_t old_val, uint8_t new_val)
{
    (void)old_val;
    mcp23017_model_t *m = (mcp23017_model_t *)user;
    uint8_t other = (reg == MCP23017_REG_IOCON_A) ? MCP23017_REG_IOCON_B
                                                  : MCP23017_REG_IOCON_A;
    m->hw.regs[other] = new_val;
}

/*
 * Replaces hw_model_i2c_read_regs, as a read of GPIO or INTCAP clears the
 * interrupt. The address steps and rolls over to 00h, or with SEQOP set
 * toggles within the A/B pair (p13).
 */
__attribute__((unused))
static bool mcp23017_hw_model_read_regs(void *ctx, uint8_t dev_addr,
                                        uint8_t reg, uint8_t *buf, uint16_t len)
{
    (void)dev_addr;
    hw_reg_model_t *hw = (hw_reg_model_t *)ctx;
    mcp23017_model_t *m = (mcp23017_model_t *)hw->user;
    uint8_t addr = reg;

    if (hw_model__should_fail(hw)) return false;
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = hw->regs[addr];
        if ((addr >= MCP23017_REG_INTCAPA && addr <= MCP23017_REG_INTCAPB) ||
            (addr >= MCP23017_REG_GPIOA && addr <= MCP23017_REG_GPIOB)) {
            mcp23017_model_clear_int(m, MCP23017_PORT_OF(addr));
        }
        if (hw->regs[MCP23017_REG_IOCON_A] & MCP23017_IOCON_SEQOP) {
            addr ^= 1u;
        } else {
            addr = (addr >= MCP23017_REG_OLATB) ? 0u : (uint8_t)(addr + 1u);
        }
    }
    return true;
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

    /* IOCON bit 0 is unimplemented */
    hw_model_set_writable_mask(&m->hw, MCP23017_REG_IOCON_A, MCP23017_IOCON_MASK);
    hw_model_set_writable_mask(&m->hw, MCP23017_REG_IOCON_B, MCP23017_IOCON_MASK);

    /* Write callbacks */
    for (uint8_t i = 0; i < 2; i++) {
        hw_model_set_on_write(&m->hw, (uint8_t)(MCP23017_REG_GPIOA + i),
                              mcp23017_model_on_gpio_write);
        hw_model_set_on_write(&m->hw, (uint8_t)(MCP23017_REG_OLATA + i),
                              mcp23017_model_on_port_write);
        hw_model_set_on_write(&m->hw, (uint8_t)(MCP23017_REG_IODIRA + i),
                              mcp23017_model_on_port_write);
        hw_model_set_on_write(&m->hw, (uint8_t)(MCP23017_REG_IPOLA + i),
                              mcp23017_model_on_port_write);
        hw_model_set_on_write(&m->hw, (uint8_t)(MCP23017_REG_GPINTENA + i),
                              mcp23017_model_on_ioc_write);
        hw_model_set_on_write(&m->hw, (uint8_t)(MCP23017_REG_DEFVALA + i),
                              mcp23017_model_on_ioc_write);
        hw_model_set_on_write(&m->hw, (uint8_t)(MCP23017_REG_INTCONA + i),
                              mcp23017_model_on_ioc_write);
        hw_model_set_on_write(&m->hw, (uint8_t)(MCP23017_REG_IOCON_A + i),
                              mcp23017_model_on_iocon_write);
    }

    /* Default pin state: all low */
    m->pin_state_a = 0x00;
    m->pin_state_b = 0x00;

    hw_model_reset(&m->hw);
}

static void mcp23017_model_set_pins(mcp23017_model_t *m, unsigned port,
                                    uint8_t value)
{
    *mcp23017_model_pins(m, port) = value;
    mcp23017_model_refresh(m, port);
    mcp23017_model_check_ioc(m, port);
}

/* Helper: simulate external pin input on port A */
__attribute__((unused))
static void mcp23017_model_set_pin_input_a(mcp23017_model_t *m, uint8_t value)
{
    mcp23017_model_set_pins(m, 0, value);
}

/* Helper: simulate external pin input on port B */
__attribute__((unused))
static void mcp23017_model_set_pin_input_b(mcp23017_model_t *m, uint8_t value)
{
    mcp23017_model_set_pins(m, 1, value);
}

/* Helper: is INTA (or INTB) active? */
__attribute__((unused))
static bool mcp23017_model_int_active(const mcp23017_model_t *m, bool port_a)
{
    if (m->hw.regs[MCP23017_REG_IOCON_A] & MCP23017_IOCON_MIRROR) {
        return m->int_pending[0] || m->int_pending[1];
    }
    return m->int_pending[port_a ? 0 : 1];
}

/* HAL delay stub */
__attribute__((unused))
static void mcp23017_model_delay(void *ctx, uint32_t ms) { (void)ctx; (void)ms; }

#endif /* HW_MODEL_MCP23017_H */
