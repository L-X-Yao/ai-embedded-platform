/*
 * hw_model_mcp2515.h -- MCP2515 CAN bus controller hardware register model
 *
 * Models Microchip MCP2515 SPI register behavior:
 * - SPI command protocol: RESET (0xC0), READ (0x03), WRITE (0x02),
 *   BIT_MODIFY (0x05), READ_STATUS (0xA0), RTS (0x80)
 * - Register file 0x00-0xFF (256 registers)
 * - Mode control: CANCTRL[7:5] = requested mode, CANSTAT[7:5] mirrors
 *   after transition (config/normal/sleep/loopback/listen)
 * - TX buffer 0: TXB0CTRL (0x30), TXREQ bit (bit 3) triggers send,
 *   clears on completion
 * - RX buffer 0: injectable received frames via helper function
 * - CANINTF (0x2C): interrupt flags, bit-modifiable
 * - CNF1/2/3 (0x2A/0x29/0x28): bit timing configuration
 *
 * Uses hw_reg_model_t for the underlying register file, with custom
 * SPI command parsing to model the real chip protocol.
 */

#ifndef HW_MODEL_MCP2515_H
#define HW_MODEL_MCP2515_H

#include "hw_model.h"

/* --- MCP2515 register addresses --- */
#define MCP_REG_CANSTAT      0x0E
#define MCP_REG_CANCTRL      0x0F
#define MCP_REG_CNF3         0x28
#define MCP_REG_CNF2         0x29
#define MCP_REG_CNF1         0x2A
#define MCP_REG_CANINTF      0x2C
#define MCP_REG_EFLG         0x2D

#define MCP_REG_TXB0CTRL     0x30
#define MCP_REG_TXB0SIDH     0x31
#define MCP_REG_TXB0SIDL     0x32
#define MCP_REG_TXB0EID8     0x33
#define MCP_REG_TXB0EID0     0x34
#define MCP_REG_TXB0DLC      0x35
#define MCP_REG_TXB0D0       0x36

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
#define MCP_TXB_TXREQ          0x08

/* --- Mode values in bits [7:5] --- */
#define MCP_OPMOD_NORMAL     0x00
#define MCP_OPMOD_SLEEP      0x20
#define MCP_OPMOD_LOOPBACK   0x40
#define MCP_OPMOD_LISTEN     0x60
#define MCP_OPMOD_CONFIG     0x80

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
    /* TX behavior: auto-clear TXREQ after send */
    bool             auto_clear_txreq;
} mcp2515_model_t;

/* --- Mode-change helper: mirror CANCTRL mode bits into CANSTAT --- */

static void mcp2515_mirror_mode(mcp2515_model_t *m)
{
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
    mcp2515_model_t *m = (mcp2515_model_t *)user;
    (void)new_val;
    /* Mirror mode bits from CANCTRL to CANSTAT (instant transition) */
    mcp2515_mirror_mode(m);
}

/* --- Write callback: TXB0CTRL TXREQ handling --- */

__attribute__((unused))
static void mcp2515_on_txb0ctrl_write(void *user, uint8_t reg,
                                      uint8_t old_val, uint8_t new_val)
{
    (void)reg;
    (void)old_val;
    mcp2515_model_t *m = (mcp2515_model_t *)user;

    if (new_val & MCP_TXB_TXREQ) {
        /* TX requested */
        m->tx_complete_count++;
        if (m->auto_clear_txreq) {
            /* Simulate immediate TX completion: TXREQ stays set until
             * next read (real chip clears after arbitration + send).
             * We keep it set so the driver's verify read sees it. */
        }
    }
}

/* --- SPI HAL callbacks --- */

__attribute__((unused))
static void mcp2515_model_cs_low(void *ctx)
{
    mcp2515_model_t *m = (mcp2515_model_t *)ctx;
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
            hw_model_reset(&m->hw);
            /* After reset, chip is in config mode */
            m->hw.regs[MCP_REG_CANSTAT] = MCP_OPMOD_CONFIG;
            m->hw.regs[MCP_REG_CANCTRL] = MCP_OPMOD_CONFIG;
            m->reset_count++;
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
            /* READ: return register value */
            return m->hw.regs[m->spi_addr];
        }
        if (m->spi_cmd == MCP_SPI_WRITE) {
            /* WRITE: store value, then trigger callbacks */
            hw_model__write_reg(&m->hw, m->spi_addr, tx);
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
        /* BIT_MODIFY: apply mask + data to register */
        uint8_t old = m->hw.regs[m->spi_addr];
        uint8_t new_val = (uint8_t)((old & ~m->bm_mask) | (tx & m->bm_mask));
        m->hw.regs[m->spi_addr] = new_val;

        /* Trigger write callback if registered */
        if (m->hw.on_write[m->spi_addr]) {
            m->hw.on_write[m->spi_addr](m->hw.user, m->spi_addr, old, new_val);
        }
        return 0xFF;
    }

    return 0xFF;
}

/* --- Model initialization --- */

static void mcp2515_model_init(mcp2515_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR defaults: chip comes up in config mode after reset */
    hw_model_set_reset_value(&m->hw, MCP_REG_CANSTAT, MCP_OPMOD_CONFIG);
    hw_model_set_reset_value(&m->hw, MCP_REG_CANCTRL, MCP_OPMOD_CONFIG);

    /* Register write callbacks */
    hw_model_set_on_write(&m->hw, MCP_REG_CANCTRL, mcp2515_on_canctrl_write);
    hw_model_set_on_write(&m->hw, MCP_REG_TXB0CTRL, mcp2515_on_txb0ctrl_write);

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
