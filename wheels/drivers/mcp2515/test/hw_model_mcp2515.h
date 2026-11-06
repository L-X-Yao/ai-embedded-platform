// SPDX-License-Identifier: MIT
/*
 * hw_model_mcp2515.h -- MCP2515 CAN bus controller hardware register model
 *
 * Models Microchip MCP2515 SPI register behavior:
 * - SPI command protocol: RESET (0xC0), READ (0x03), WRITE (0x02),
 *   BIT_MODIFY (0x05), READ_STATUS (0xA0), RTS (0x80)
 * - Register file 0x00-0xFF (256 registers)
 * - Mode control: CANCTRL[7:5] = requested mode, CANSTAT[7:5] mirrors
 *   after transition (config/normal/sleep/loopback/listen), held while any
 *   TXREQ is set
 * - TX buffers: TXREQ (TXBnCTRL bit 3) stays set until
 *   mcp2515_model_finish_tx(), or at once with auto_clear_txreq, and clears
 *   with TXnIF set, in Normal and Loopback mode only
 * - CNF, TXRTSCTRL, filter and mask writes need Configuration mode, and filters
 *   and masks read 0 in any other mode
 * - Read-only bits, the address pointer of READ and WRITE, and BIT MODIFY
 *   acting as a write on registers that are not bit-modifiable
 * - RX buffer 0: injectable received frames via helper function
 * - CANINTF (0x2C): interrupt flags, bit-modifiable
 * - CNF1/2/3 (0x2A/0x29/0x28): bit timing configuration
 * - OST after a RESET: SPI operations before it expires are counted
 * - Sleep: the oscillator is stopped, so CANCTRL requests do nothing until
 *   WAKIF is set with WAKIE set, or mcp2515_model_bus_activity() with WAKIE set.
 *   The chip then wakes in Listen-Only mode, and another OST runs
 *
 * Uses hw_reg_model_t for the underlying register file, with custom
 * SPI command parsing to model the real chip protocol.
 */

#ifndef HW_MODEL_MCP2515_H
#define HW_MODEL_MCP2515_H

#include "hw_model.h"

/* --- MCP2515 register addresses --- */
#define MCP_REG_TXRTSCTRL    0x0D
#define MCP_REG_CANSTAT      0x0E
#define MCP_REG_CANCTRL      0x0F
#define MCP_REG_CNF3         0x28
#define MCP_REG_CNF2         0x29
#define MCP_REG_CNF1         0x2A
#define MCP_REG_CANINTE      0x2B
#define MCP_REG_CANINTF      0x2C
#define MCP_REG_EFLG         0x2D

#define MCP_REG_TXB0CTRL     0x30
#define MCP_REG_TXB0SIDH     0x31
#define MCP_REG_TXB0SIDL     0x32
#define MCP_REG_TXB0EID8     0x33
#define MCP_REG_TXB0EID0     0x34
#define MCP_REG_TXB0DLC      0x35
#define MCP_REG_TXB0D0       0x36
#define MCP_REG_TXB1CTRL     0x40
#define MCP_REG_TXB2CTRL     0x50

#define MCP_REG_RXB0CTRL     0x60
#define MCP_REG_RXB0SIDH     0x61
#define MCP_REG_RXB0SIDL     0x62
#define MCP_REG_RXB0EID8     0x63
#define MCP_REG_RXB0EID0     0x64
#define MCP_REG_RXB0DLC      0x65
#define MCP_REG_RXB0D0       0x66

/* Filter/Mask base registers */
#define MCP_REG_RXF0SIDH     0x00
#define MCP_REG_RXM0SIDH     0x20
#define MCP_REG_RXM1SIDH     0x24

/* --- Bit fields --- */
#define MCP_CANCTRL_REQOP_MASK 0xE0
#define MCP_CANSTAT_OPMOD_MASK 0xE0
#define MCP_CANINTF_RX0IF      0x01
#define MCP_CANINTF_RX1IF      0x02
#define MCP_CANINTF_TX0IF      0x04
#define MCP_CANINTF_WAKIF      0x40
#define MCP_CANINTE_WAKIE      0x40
#define MCP_TXB_TXREQ          0x08

/* --- Mode values in bits [7:5] --- */
#define MCP_OPMOD_NORMAL     0x00
#define MCP_OPMOD_SLEEP      0x20
#define MCP_OPMOD_LOOPBACK   0x40
#define MCP_OPMOD_LISTEN     0x60
#define MCP_OPMOD_CONFIG     0x80

/* Oscillator Start-up Timer: 128 OSC1 cycles (DS20001801J p.55). The model uses
 * the slowest clock the chip allows, TOSC max 1000 ns (Table 13-2), so a driver
 * that waits for a faster clock than it was told about fails here. */
#define MCP_OST_US           128u

/* --- SPI commands --- */
#define MCP_SPI_RESET        0xC0
#define MCP_SPI_READ         0x03
#define MCP_SPI_WRITE        0x02
#define MCP_SPI_BIT_MODIFY   0x05
#define MCP_SPI_READ_STATUS  0xA0
#define MCP_SPI_RTS          0x80

typedef enum {
    MCP_PHASE_CMD = 0,
    MCP_PHASE_ADDR,
    MCP_PHASE_DATA,
    MCP_PHASE_BM_MASK,     /* BIT_MODIFY: waiting for mask byte */
    MCP_PHASE_BM_DATA,     /* BIT_MODIFY: waiting for data byte */
} mcp2515_phase_t;

typedef struct {
    hw_reg_model_t   hw;
    /* SPI transaction state */
    mcp2515_phase_t  phase;
    uint8_t          spi_cmd;
    uint8_t          spi_addr;
    uint8_t          bm_mask;
    /* Status byte for READ_STATUS command */
    uint8_t          status_byte;
    /* Counters */
    int              reset_count;
    int              tx_complete_count;
    int              tx_buffer_writes_pending;
    /* OST: the model has no clock of its own, only the driver's waits count */
    uint32_t         ost_left_us;
    int              spi_during_ost;
    uint32_t         delay_us_total;
    /* TX behavior: auto-clear TXREQ after send */
    bool             auto_clear_txreq;
} mcp2515_model_t;

/* --- What the chip does with an address --- */

static uint8_t mcp2515_opmod(const mcp2515_model_t *m)
{
    return (uint8_t)(m->hw.regs[MCP_REG_CANSTAT] & MCP_CANSTAT_OPMOD_MASK);
}

static bool mcp2515_is_mask_or_filter(uint8_t addr)
{
    return addr <= 0x0Bu ||                  /* RXF0 to RXF2 */
           (addr >= 0x10u && addr <= 0x1Bu) || /* RXF3 to RXF5 */
           (addr >= 0x20u && addr <= 0x27u); /* RXM0, RXM1 */
}

/* p.59: "Configuration mode is the only mode where the following registers are
 * modifiable: CNF1, CNF2, CNF3 registers, TXRTSCTRL register, Filter registers,
 * Mask registers" */
static bool mcp2515_is_config_only(uint8_t addr)
{
    return mcp2515_is_mask_or_filter(addr) || addr == MCP_REG_TXRTSCTRL ||
           addr == MCP_REG_CNF3 || addr == MCP_REG_CNF2 || addr == MCP_REG_CNF1;
}

/* p.63: the shaded locations of Table 11-1. BIT MODIFY on any other register
 * "will force the mask to FFh". */
static bool mcp2515_is_bit_modifiable(uint8_t addr)
{
    switch (addr) {
    case 0x0C: case MCP_REG_TXRTSCTRL: case MCP_REG_CANSTAT: case MCP_REG_CANCTRL:
    case MCP_REG_CNF3: case MCP_REG_CNF2: case MCP_REG_CNF1:
    case MCP_REG_CANINTE: case MCP_REG_CANINTF: case MCP_REG_EFLG:
    case MCP_REG_TXB0CTRL: case MCP_REG_TXB1CTRL: case MCP_REG_TXB2CTRL:
    case MCP_REG_RXB0CTRL: case 0x70:
        return true;
    default:
        return false;
    }
}

static bool mcp2515_tx_pending(const mcp2515_model_t *m)
{
    return ((m->hw.regs[MCP_REG_TXB0CTRL] | m->hw.regs[MCP_REG_TXB1CTRL] |
             m->hw.regs[MCP_REG_TXB2CTRL]) & MCP_TXB_TXREQ) != 0;
}

/* --- Mode change: CANCTRL mode bits reach CANSTAT only when the chip can act --- */

static void mcp2515_mirror_mode(mcp2515_model_t *m)
{
    /* DS20001801J p.59: "When in Sleep mode, the MCP2515 stops its internal
     * oscillator", so nothing acts on the request until the chip wakes */
    if (mcp2515_opmod(m) == MCP_OPMOD_SLEEP) {
        return;
    }
    /* p.59: "the mode will not actually change until all pending message
     * transmissions are complete" */
    if (mcp2515_tx_pending(m)) {
        return;
    }
    uint8_t requested = m->hw.regs[MCP_REG_CANCTRL] & MCP_CANCTRL_REQOP_MASK;
    m->hw.regs[MCP_REG_CANSTAT] =
        (uint8_t)((m->hw.regs[MCP_REG_CANSTAT] & ~MCP_CANSTAT_OPMOD_MASK) |
                  requested);
}

/* --- Write callback: CANCTRL mode change --- */

__attribute__((unused))
static void mcp2515_on_canctrl_write(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    (void)old_val;
    (void)new_val;
    mcp2515_mirror_mode((mcp2515_model_t *)user);
}

/* --- Wake-up --- */

/* p.59: the chip "will wake-up in Listen-Only mode". p.55: the OST runs again
 * after a wake-up from Sleep. */
static void mcp2515_wake(mcp2515_model_t *m)
{
    uint8_t *canstat = &m->hw.regs[MCP_REG_CANSTAT];

    *canstat = (uint8_t)((*canstat & ~MCP_CANSTAT_OPMOD_MASK) | MCP_OPMOD_LISTEN);
    m->ost_left_us = MCP_OST_US;
}

/* --- Write callback: CANINTF WAKIF wakes the chip --- */

/* p.59: the chip wakes "when the MCU sets, via the SPI interface, the WAKIF bit",
 * and "the WAKIE bit (CANINTE[6]) must also be set". */
__attribute__((unused))
static void mcp2515_on_canintf_write(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    mcp2515_model_t *m = (mcp2515_model_t *)user;

    if (mcp2515_opmod(m) == MCP_OPMOD_SLEEP &&
        (m->hw.regs[MCP_REG_CANINTE] & MCP_CANINTE_WAKIE) &&
        (new_val & MCP_CANINTF_WAKIF) && !(old_val & MCP_CANINTF_WAKIF)) {
        mcp2515_wake(m);
    }
}

/* p.59: "The MCP2515 will wake-up when bus activity occurs". "If the WAKIE bit is
 * set, the device will wake-up and generate an interrupt." */
__attribute__((unused))
static void mcp2515_model_bus_activity(mcp2515_model_t *m)
{
    if (mcp2515_opmod(m) == MCP_OPMOD_SLEEP &&
        (m->hw.regs[MCP_REG_CANINTE] & MCP_CANINTE_WAKIE)) {
        m->hw.regs[MCP_REG_CANINTF] |= MCP_CANINTF_WAKIF;
        mcp2515_wake(m);
    }
}

/* --- Transmission --- */

/* p.15: "Once the transmission has completed successfully, the TXREQ bit will be
 * cleared, the TXnIF bit (CANINTF) will be set". p.59: Listen-Only sends nothing
 * and p.60: Normal is "the only mode in which the MCP2515 will transmit messages
 * over the CAN bus", while Loopback passes frames to the receive buffers. A held
 * mode request acts as soon as nothing is pending. */
__attribute__((unused))
static void mcp2515_model_finish_tx(mcp2515_model_t *m)
{
    static const struct { uint8_t ctrl; uint8_t flag; } buf[] = {
        { MCP_REG_TXB0CTRL, MCP_CANINTF_TX0IF },
        { MCP_REG_TXB1CTRL, 0x08 },
        { MCP_REG_TXB2CTRL, 0x10 },
    };

    if (mcp2515_opmod(m) != MCP_OPMOD_NORMAL &&
        mcp2515_opmod(m) != MCP_OPMOD_LOOPBACK) {
        return;
    }
    for (size_t i = 0; i < sizeof(buf) / sizeof(buf[0]); i++) {
        if (m->hw.regs[buf[i].ctrl] & MCP_TXB_TXREQ) {
            m->hw.regs[buf[i].ctrl] &= (uint8_t)~MCP_TXB_TXREQ;
            m->hw.regs[MCP_REG_CANINTF] |= buf[i].flag;
        }
    }
    mcp2515_mirror_mode(m);
}

/* --- Write callback: TXBnCTRL TXREQ handling --- */

__attribute__((unused))
static void mcp2515_on_txbctrl_write(void *user, uint8_t reg,
                                     uint8_t old_val, uint8_t new_val)
{
    mcp2515_model_t *m = (mcp2515_model_t *)user;

    if ((new_val & MCP_TXB_TXREQ) && !(old_val & MCP_TXB_TXREQ)) {
        /* p.15: "When TXREQ is set, the ABTF, MLOA and TXERR bits (TXBnCTRL[5:4])
         * will be cleared automatically." */
        m->hw.regs[reg] &= (uint8_t)~0x70u;
        m->tx_complete_count++;
        if (m->auto_clear_txreq) {
            mcp2515_model_finish_tx(m);
        }
    } else if (!(new_val & MCP_TXB_TXREQ)) {
        /* p.16: clearing TXREQ aborts the message, so it is no longer pending */
        mcp2515_mirror_mode(m);
    }
}

/* --- Register access over SPI --- */

static uint8_t mcp2515_model_read(const mcp2515_model_t *m, uint8_t addr)
{
    /* Note 1 of the RXFn and RXMn registers: "The Mask and Filter registers read
     * all '0's when in any mode except Configuration mode." */
    if (mcp2515_is_mask_or_filter(addr) && mcp2515_opmod(m) != MCP_OPMOD_CONFIG) {
        return 0;
    }
    return m->hw.regs[addr];
}

static void mcp2515_model_write(mcp2515_model_t *m, uint8_t addr, uint8_t val)
{
    if (mcp2515_is_config_only(addr) && mcp2515_opmod(m) != MCP_OPMOD_CONFIG) {
        return;
    }
    /* p.15 note: "The TXREQ bit (TXBnCTRL[3]) must be clear (indicating the
     * transmit buffer is not pending transmission) before writing to the
     * transmit buffer." Address 0xn0 is the control register itself. */
    if (addr >= 0x30u && addr <= 0x5Du && (addr & 0x0Fu) >= 0x01u &&
        (addr & 0x0Fu) <= 0x0Du &&
        (m->hw.regs[addr & 0xF0u] & MCP_TXB_TXREQ)) {
        m->tx_buffer_writes_pending++;
    }
    hw_model__write_reg((hw_reg_model_t *)&m->hw, addr, val);
}

/* --- SPI HAL callbacks --- */

__attribute__((unused))
static void mcp2515_model_cs_low(void *ctx)
{
    mcp2515_model_t *m = (mcp2515_model_t *)ctx;
    if (m->ost_left_us > 0u) {
        m->spi_during_ost++;
    }
    m->phase = MCP_PHASE_CMD;
    m->spi_cmd = 0;
    m->spi_addr = 0;
    m->bm_mask = 0;
}

__attribute__((unused))
static void mcp2515_model_cs_high(void *ctx)
{
    (void)ctx;
    /* Transaction complete */
}

__attribute__((unused))
static uint8_t mcp2515_model_xfer(void *ctx, uint8_t tx)
{
    mcp2515_model_t *m = (mcp2515_model_t *)ctx;

    if (m->phase == MCP_PHASE_CMD) {
        m->spi_cmd = tx;

        /* RESET command: immediate effect, no more bytes */
        if (tx == MCP_SPI_RESET) {
            /* Back to the Table 11-2 values, which is Configuration mode */
            hw_model_reset(&m->hw);
            m->reset_count++;
            m->ost_left_us = MCP_OST_US;
            return 0xFF;
        }

        /* READ_STATUS: next byte(s) return status */
        if (tx == MCP_SPI_READ_STATUS) {
            m->phase = MCP_PHASE_DATA;
            return 0xFF;
        }

        /* All other commands need at least an address byte */
        m->phase = MCP_PHASE_ADDR;
        return 0xFF;
    }

    if (m->phase == MCP_PHASE_ADDR) {
        m->spi_addr = tx;

        if (m->spi_cmd == MCP_SPI_BIT_MODIFY) {
            /* BIT_MODIFY: next byte is mask */
            m->phase = MCP_PHASE_BM_MASK;
        } else {
            m->phase = MCP_PHASE_DATA;
        }
        return 0xFF;
    }

    if (m->phase == MCP_PHASE_DATA) {
        if (m->spi_cmd == MCP_SPI_READ) {
            /* READ: the address pointer moves on after each byte */
            return mcp2515_model_read(m, m->spi_addr++);
        }
        if (m->spi_cmd == MCP_SPI_WRITE) {
            /* WRITE: runs on to the next register like READ */
            mcp2515_model_write(m, m->spi_addr++, tx);
            return 0xFF;
        }
        if (m->spi_cmd == MCP_SPI_READ_STATUS) {
            /* READ_STATUS: return the pre-set status byte */
            return m->status_byte;
        }
        return 0xFF;
    }

    if (m->phase == MCP_PHASE_BM_MASK) {
        m->bm_mask = tx;
        m->phase = MCP_PHASE_BM_DATA;
        return 0xFF;
    }

    if (m->phase == MCP_PHASE_BM_DATA) {
        /* BIT_MODIFY: section 12.10 note, on registers that are not
         * bit-modifiable it will "force the mask to FFh" */
        uint8_t mask = mcp2515_is_bit_modifiable(m->spi_addr) ? m->bm_mask : 0xFFu;
        uint8_t old = m->hw.regs[m->spi_addr];
        mcp2515_model_write(m, m->spi_addr,
                            (uint8_t)((old & ~mask) | (tx & mask)));
        return 0xFF;
    }

    return 0xFF;
}

__attribute__((unused))
static void mcp2515_model_delay_us(void *ctx, uint32_t us)
{
    mcp2515_model_t *m = (mcp2515_model_t *)ctx;
    m->delay_us_total += us;
    m->ost_left_us = (us >= m->ost_left_us) ? 0u : m->ost_left_us - us;
}

/* --- Model initialization --- */

static void mcp2515_model_init(mcp2515_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* p.63 Table 11-2: the chip comes up in Configuration mode */
    hw_model_set_reset_value(&m->hw, MCP_REG_CANSTAT, MCP_OPMOD_CONFIG);
    hw_model_set_reset_value(&m->hw, MCP_REG_CANCTRL, 0x87);

    /* Bits marked R or U in the register tables */
    hw_model_set_readonly(&m->hw, MCP_REG_CANSTAT);
    hw_model_set_readonly(&m->hw, 0x1C);                        /* TEC */
    hw_model_set_readonly(&m->hw, 0x1D);                        /* REC */
    hw_model_set_writable_mask(&m->hw, 0x0C, 0x3F);             /* BFPCTRL */
    hw_model_set_writable_mask(&m->hw, MCP_REG_TXRTSCTRL, 0x07);
    hw_model_set_writable_mask(&m->hw, MCP_REG_CNF3, 0xC7);
    hw_model_set_writable_mask(&m->hw, MCP_REG_EFLG, 0xC0);
    hw_model_set_writable_mask(&m->hw, MCP_REG_RXB0CTRL, 0x64);
    hw_model_set_writable_mask(&m->hw, 0x70, 0x60);             /* RXB1CTRL */
    for (uint8_t base = MCP_REG_TXB0CTRL; base <= MCP_REG_TXB2CTRL; base += 0x10u) {
        hw_model_set_writable_mask(&m->hw, base, 0x0B);                /* TXBnCTRL */
        hw_model_set_writable_mask(&m->hw, (uint8_t)(base + 2), 0xEB); /* TXBnSIDL */
        hw_model_set_writable_mask(&m->hw, (uint8_t)(base + 5), 0x4F); /* TXBnDLC */
        hw_model_set_on_write(&m->hw, base, mcp2515_on_txbctrl_write);
    }
    for (uint8_t i = 1; i <= 0x0Du; i++) {
        hw_model_set_readonly(&m->hw, (uint8_t)(MCP_REG_RXB0CTRL + i));
        hw_model_set_readonly(&m->hw, (uint8_t)(0x70u + i));
    }
    /* RXFnSIDL has EXIDE, RXMnSIDL has not: bits 4:2 are U-0 there (p.37) */
    for (uint8_t i = 0; i < 6u; i++) {
        static const uint8_t sidl[] = { 0x01, 0x05, 0x09, 0x11, 0x15, 0x19 };
        hw_model_set_writable_mask(&m->hw, sidl[i], 0xEB);
    }
    hw_model_set_writable_mask(&m->hw, 0x21, 0xE3);
    hw_model_set_writable_mask(&m->hw, 0x25, 0xE3);

    /* Register write callbacks */
    hw_model_set_on_write(&m->hw, MCP_REG_CANCTRL, mcp2515_on_canctrl_write);
    hw_model_set_on_write(&m->hw, MCP_REG_CANINTF, mcp2515_on_canintf_write);

    hw_model_reset(&m->hw);

    m->auto_clear_txreq = false;
}

/* --- Helper: inject a standard CAN frame into RXB0 --- */

__attribute__((unused))
static void mcp2515_model_inject_rx_std(mcp2515_model_t *m, uint32_t id,
                                        const uint8_t *data, uint8_t dlc)
{
    m->hw.regs[MCP_REG_RXB0SIDH] = (uint8_t)(id >> 3);
    m->hw.regs[MCP_REG_RXB0SIDL] = (uint8_t)((id << 5) & 0xE0);  /* IDE=0 */
    m->hw.regs[MCP_REG_RXB0EID8] = 0;
    m->hw.regs[MCP_REG_RXB0EID0] = 0;
    m->hw.regs[MCP_REG_RXB0DLC] = dlc;
    for (uint8_t i = 0; i < dlc && i < 8; i++) {
        m->hw.regs[(uint8_t)(MCP_REG_RXB0D0 + i)] = data[i];
    }
    /* Set RX0IF */
    m->hw.regs[MCP_REG_CANINTF] |= MCP_CANINTF_RX0IF;
}

/* --- Helper: inject an extended CAN frame into RXB0 --- */

__attribute__((unused))
static void mcp2515_model_inject_rx_ext(mcp2515_model_t *m, uint32_t id,
                                        const uint8_t *data, uint8_t dlc)
{
    uint16_t sid = (uint16_t)(id >> 18);
    uint32_t eid = id & 0x3FFFFu;

    m->hw.regs[MCP_REG_RXB0SIDH] = (uint8_t)(sid >> 3);
    m->hw.regs[MCP_REG_RXB0SIDL] =
        (uint8_t)(((sid & 0x07u) << 5) | 0x08 | ((eid >> 16) & 0x03u));
    m->hw.regs[MCP_REG_RXB0EID8] = (uint8_t)(eid >> 8);
    m->hw.regs[MCP_REG_RXB0EID0] = (uint8_t)(eid);
    m->hw.regs[MCP_REG_RXB0DLC] = dlc;
    for (uint8_t i = 0; i < dlc && i < 8; i++) {
        m->hw.regs[(uint8_t)(MCP_REG_RXB0D0 + i)] = data[i];
    }
    m->hw.regs[MCP_REG_CANINTF] |= MCP_CANINTF_RX0IF;
}

/* --- Helper: inject a standard CAN frame into RXB1 --- */

__attribute__((unused))
static void mcp2515_model_inject_rx1_std(mcp2515_model_t *m, uint32_t id,
                                         const uint8_t *data, uint8_t dlc)
{
    /* RXB1 base registers: 0x71-0x76 */
    m->hw.regs[0x71] = (uint8_t)(id >> 3);
    m->hw.regs[0x72] = (uint8_t)((id << 5) & 0xE0);  /* IDE=0 */
    m->hw.regs[0x73] = 0;
    m->hw.regs[0x74] = 0;
    m->hw.regs[0x75] = dlc;
    for (uint8_t i = 0; i < dlc && i < 8; i++) {
        m->hw.regs[(uint8_t)(0x76u + i)] = data[i];
    }
    /* Set RX1IF (bit 1), clear RX0IF */
    m->hw.regs[MCP_REG_CANINTF] =
        (uint8_t)((m->hw.regs[MCP_REG_CANINTF] & ~MCP_CANINTF_RX0IF) |
                  MCP_CANINTF_RX1IF);
}

#endif /* HW_MODEL_MCP2515_H */
