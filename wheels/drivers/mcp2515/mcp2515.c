// SPDX-License-Identifier: MIT
/*
 * mcp2515 -- SPI CAN bus controller driver for Microchip MCP2515
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "mcp2515.h"
#include "contract.h"
#include <string.h>

/* ---------- SPI instructions ---------- */

#define SPI_RESET       0xC0
#define SPI_READ        0x03
#define SPI_WRITE       0x02
#define SPI_RTS         0x80
#define SPI_READ_STATUS 0xA0
#define SPI_BIT_MODIFY  0x05

/* ---------- registers ---------- */

#define REG_CANSTAT     0x0E
#define REG_CANCTRL     0x0F
#define REG_CNF3        0x28
#define REG_CNF2        0x29
#define REG_CNF1        0x2A
#define REG_CANINTE     0x2B
#define REG_CANINTF     0x2C
#define REG_EFLG        0x2D

#define REG_TXB0CTRL    0x30
#define REG_TXB0SIDH    0x31
#define REG_TXB0SIDL    0x32
#define REG_TXB0EID8    0x33
#define REG_TXB0EID0    0x34
#define REG_TXB0DLC     0x35
#define REG_TXB0D0      0x36

#define REG_RXB0CTRL    0x60
#define REG_RXB0SIDH    0x61
#define REG_RXB0SIDL    0x62
#define REG_RXB0EID8    0x63
#define REG_RXB0EID0    0x64
#define REG_RXB0DLC     0x65
#define REG_RXB0D0      0x66

#define REG_RXB1CTRL    0x70
#define REG_RXB1SIDH    0x71
#define REG_RXB1SIDL    0x72
#define REG_RXB1EID8    0x73
#define REG_RXB1EID0    0x74
#define REG_RXB1DLC     0x75
#define REG_RXB1D0      0x76

/* Filter registers: RXF0-RXF5 */
#define REG_RXF0SIDH    0x00
#define REG_RXF1SIDH    0x04
#define REG_RXF2SIDH    0x08
#define REG_RXF3SIDH    0x10
#define REG_RXF4SIDH    0x14
#define REG_RXF5SIDH    0x18

/* Mask registers: RXM0-RXM1 */
#define REG_RXM0SIDH    0x20
#define REG_RXM1SIDH    0x24

/* ---------- bit fields ---------- */

#define CANCTRL_REQOP_MASK  0xE0
#define CANSTAT_OPMOD_MASK  0xE0
#define CANINTF_RX0IF       0x01
#define CANINTF_RX1IF       0x02
#define CANINTF_WAKIF       0x40
#define CANINTE_WAKIE       0x40
#define TXB_TXREQ           0x08
#define TXB_EXIDE            0x08
#define RXB_IDE              0x08
#define RXB_SRR              0x10

/* Mode values in CANCTRL/CANSTAT bits [7:5] */
#define OPMOD_NORMAL    0x00
#define OPMOD_SLEEP     0x20
#define OPMOD_LOOPBACK  0x40
#define OPMOD_LISTEN    0x60
#define OPMOD_CONFIG    0x80

/* Oscillator Start-up Timer (DS20001801J p.55): 128 OSC1 cycles after a Reset or
 * a wake-up, and no SPI operation until it has expired. The driver is not told
 * the oscillator, so it waits for the slowest: TOSC max 1000 ns (Table 13-2). */
#define OST_WAIT_US     128u

/* ---------- CNF values for 8 MHz crystal ---------- */

/*
 * Bit timing for 8 MHz oscillator, TQ = 2 x (BRP + 1) / FOSC:
 *   125 kbps: TQ=1000ns, SJW=1, BRP=3,  PS1=3, PS2=3, PROPSEG=1
 *   250 kbps: TQ=500ns,  SJW=1, BRP=1,  PS1=3, PS2=3, PROPSEG=1
 *   500 kbps: TQ=250ns,  SJW=1, BRP=0,  PS1=3, PS2=3, PROPSEG=1
 * 1 Mbps needs a faster oscillator: at 250 ns a bit has only 4 TQ, too few
 * for the 2 TQ minimum of PS2.
 */

typedef struct {
    uint8_t cnf1;
    uint8_t cnf2;
    uint8_t cnf3;
} cnf_set_t;

static const cnf_set_t cnf_table[] = {
    [MCP2515_SPEED_125KBPS] = { 0x03, 0x90, 0x02 },  /* BRP=3, PS1=2, PROPSEG=0, PS2=2 */
    [MCP2515_SPEED_250KBPS] = { 0x01, 0x90, 0x02 },  /* BRP=1 */
    [MCP2515_SPEED_500KBPS] = { 0x00, 0x90, 0x02 },  /* BRP=0 */
};

/* ---------- low-level SPI helpers ---------- */

static void reg_write(const mcp2515_spi_t *s, uint8_t addr, uint8_t val)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, SPI_WRITE);
    s->xfer(s->ctx, addr);
    s->xfer(s->ctx, val);
    s->cs_high(s->ctx);
}

static uint8_t reg_read(const mcp2515_spi_t *s, uint8_t addr)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, SPI_READ);
    s->xfer(s->ctx, addr);
    uint8_t val = s->xfer(s->ctx, 0xFF);
    s->cs_high(s->ctx);
    return val;
}

static void bit_modify(const mcp2515_spi_t *s, uint8_t addr,
                        uint8_t mask, uint8_t val)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, SPI_BIT_MODIFY);
    s->xfer(s->ctx, addr);
    s->xfer(s->ctx, mask);
    s->xfer(s->ctx, val);
    s->cs_high(s->ctx);
}

static void reset_cmd(const mcp2515_spi_t *s)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, SPI_RESET);
    s->cs_high(s->ctx);
}

static uint8_t read_status_cmd(const mcp2515_spi_t *s)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, SPI_READ_STATUS);
    uint8_t st = s->xfer(s->ctx, 0xFF);
    s->cs_high(s->ctx);
    return st;
}

static uint8_t mode_to_opmod(mcp2515_mode_t m)
{
    switch (m) {
    case MCP2515_MODE_NORMAL:   return OPMOD_NORMAL;
    case MCP2515_MODE_SLEEP:    return OPMOD_SLEEP;
    case MCP2515_MODE_LOOPBACK: return OPMOD_LOOPBACK;
    case MCP2515_MODE_LISTEN:   return OPMOD_LISTEN;
    case MCP2515_MODE_CONFIG:   return OPMOD_CONFIG;
    default:                    return OPMOD_NORMAL;
    }
}

static mcp2515_err_t wait_mode(const mcp2515_spi_t *s, uint8_t expected)
{
    uint32_t n = MCP2515_MODE_TIMEOUT;
    while (n--) {
        uint8_t stat = reg_read(s, REG_CANSTAT);
        if ((stat & CANSTAT_OPMOD_MASK) == expected)
            return MCP2515_OK;
    }
    return MCP2515_ERR_TIMEOUT;
}

/*
 * Wake from Sleep (DS20001801J p.59). The oscillator is stopped, so a mode
 * request alone never takes effect: the chip wakes when the MCU sets WAKIF with
 * WAKIE also set, then runs the OST and comes up in Listen-Only mode. Returns
 * CANINTE as it was, for wake_finish.
 */
static uint8_t wake_start(const mcp2515_spi_t *s)
{
    uint8_t inte = reg_read(s, REG_CANINTE);

    bit_modify(s, REG_CANINTE, CANINTE_WAKIE, CANINTE_WAKIE);
    bit_modify(s, REG_CANINTF, CANINTF_WAKIF, CANINTF_WAKIF);
    s->delay_us(s->ctx, OST_WAIT_US);
    return inte;
}

/* After the mode change has settled: leave no wake-up interrupt behind, and
 * clear WAKIF even after a timeout so that the next wake-up can set it again */
static void wake_finish(const mcp2515_spi_t *s, uint8_t inte)
{
    bit_modify(s, REG_CANINTF, CANINTF_WAKIF, 0x00);
    bit_modify(s, REG_CANINTE, CANINTE_WAKIE, (uint8_t)(inte & CANINTE_WAKIE));
}

/* ---------- ID encoding helpers ---------- */

/*
 * Extended CAN ID (29-bit) layout in MCP2515 registers:
 *   SID[10:0]  = id[28:18]   (11 bits — standard identifier part)
 *   EID[17:0]  = id[17:0]    (18 bits — extended identifier part)
 *
 * TXBnSIDH / RXBnSIDH (base+0):
 *   [7:0] = SID[10:3]
 *
 * TXBnSIDL / RXBnSIDL (base+1):
 *   [7:5] = SID[2:0]
 *   [3]   = EXIDE (extended identifier enable)
 *   [1:0] = EID[17:16]
 *
 * TXBnEID8 / RXBnEID8 (base+2):
 *   [7:0] = EID[15:8]
 *
 * TXBnEID0 / RXBnEID0 (base+3):
 *   [7:0] = EID[7:0]
 */

static void encode_id(const mcp2515_spi_t *s, uint8_t base,
                       uint32_t id, bool extended)
{
    if (extended) {
        uint16_t sid = (uint16_t)(id >> 18);       /* SID[10:0] */
        uint32_t eid = id & 0x3FFFFu;              /* EID[17:0] */

        uint8_t sidh = (uint8_t)(sid >> 3);        /* SID[10:3] */
        uint8_t sidl = (uint8_t)(((sid & 0x07u) << 5) |   /* SID[2:0] → bits[7:5] */
                                 TXB_EXIDE |                /* EXIDE → bit[3]       */
                                 ((eid >> 16) & 0x03u));    /* EID[17:16] → bits[1:0] */
        uint8_t eid8 = (uint8_t)(eid >> 8);        /* EID[15:8] */
        uint8_t eid0 = (uint8_t)(eid);             /* EID[7:0]  */

        reg_write(s, base,                    sidh);
        reg_write(s, (uint8_t)(base + 1),     sidl);
        reg_write(s, (uint8_t)(base + 2),     eid8);
        reg_write(s, (uint8_t)(base + 3),     eid0);
    } else {
        reg_write(s, base, (uint8_t)(id >> 3));
        reg_write(s, (uint8_t)(base + 1), (uint8_t)((id << 5) & 0xE0));
        reg_write(s, (uint8_t)(base + 2), 0);
        reg_write(s, (uint8_t)(base + 3), 0);
    }
}

static void decode_id(const mcp2515_spi_t *s, uint8_t base,
                       uint32_t *id, bool *extended)
{
    uint8_t sidh = reg_read(s, base);
    uint8_t sidl = reg_read(s, (uint8_t)(base + 1));

    if (sidl & RXB_IDE) {
        uint8_t eid8 = reg_read(s, (uint8_t)(base + 2));
        uint8_t eid0 = reg_read(s, (uint8_t)(base + 3));
        *extended = true;

        uint16_t sid = (uint16_t)((uint16_t)sidh << 3 |
                       (uint16_t)((sidl >> 5) & 0x07u));     /* SID[10:0] */
        uint32_t eid = ((uint32_t)(sidl & 0x03u) << 16) |   /* EID[17:16] */
                       ((uint32_t)eid8 << 8) |               /* EID[15:8]  */
                       (uint32_t)eid0;                       /* EID[7:0]   */

        *id = ((uint32_t)sid << 18) | eid;
    } else {
        *extended = false;
        *id = ((uint32_t)sidh << 3) | ((uint32_t)(sidl >> 5) & 0x07);
    }
}

#define FILTER_COUNT 6u

static const uint8_t filter_base[FILTER_COUNT] = {
    REG_RXF0SIDH, REG_RXF1SIDH, REG_RXF2SIDH,
    REG_RXF3SIDH, REG_RXF4SIDH, REG_RXF5SIDH,
};

/* ---------- public API ---------- */

/* The steps of init that talk to the chip */
static mcp2515_err_t bring_up(mcp2515_t *dev, const mcp2515_spi_t *spi,
                              mcp2515_speed_t speed)
{
    /* hardware reset */
    reset_cmd(spi);
    spi->delay_us(spi->ctx, OST_WAIT_US);

    /* chip enters config mode after reset — verify */
    mcp2515_err_t err = wait_mode(spi, OPMOD_CONFIG);
    if (err != MCP2515_OK) return err;

    dev->mode = MCP2515_MODE_CONFIG;

    /* set CAN speed */
    const cnf_set_t *c = &cnf_table[speed];
    reg_write(spi, REG_CNF1, c->cnf1);
    reg_write(spi, REG_CNF2, c->cnf2);
    reg_write(spi, REG_CNF3, c->cnf3);

    /* clear all interrupt flags */
    reg_write(spi, REG_CANINTF, 0x00);

    /* accept all messages by default (masks = 0) */
    reg_write(spi, REG_RXM0SIDH, 0x00);
    reg_write(spi, (uint8_t)(REG_RXM0SIDH + 1), 0x00);
    reg_write(spi, (uint8_t)(REG_RXM0SIDH + 2), 0x00);
    reg_write(spi, (uint8_t)(REG_RXM0SIDH + 3), 0x00);
    reg_write(spi, REG_RXM1SIDH, 0x00);
    reg_write(spi, (uint8_t)(REG_RXM1SIDH + 1), 0x00);
    reg_write(spi, (uint8_t)(REG_RXM1SIDH + 2), 0x00);
    reg_write(spi, (uint8_t)(REG_RXM1SIDH + 3), 0x00);

    /* p.35: EXIDE gives each filter one frame format and is unknown after POR,
     * and the masks have no such bit. So zero filters of both formats go on
     * each buffer: RXF0 and RXF1 on RXB0, the other four on RXB1. */
    for (uint8_t i = 0; i < FILTER_COUNT; i++) {
        encode_id(spi, filter_base[i], 0, (i & 1u) != 0u);
    }

    /* switch to normal mode */
    return mcp2515_set_mode(dev, MCP2515_MODE_NORMAL);
}

mcp2515_err_t mcp2515_init(mcp2515_t *dev, const mcp2515_spi_t *spi,
                            mcp2515_speed_t speed)
{
    if (!dev)
        return MCP2515_ERR_SPI;

    /* dev->spi stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!spi || !spi->cs_low || !spi->cs_high || !spi->xfer)
        return MCP2515_ERR_SPI;
    if (speed >= MCP2515_SPEED_1MBPS)
        return MCP2515_ERR_PARAM;
    if (!spi->delay_us)
        return MCP2515_ERR_PARAM;

    dev->spi = spi;

    mcp2515_err_t err = bring_up(dev, spi, speed);
    if (err != MCP2515_OK)
        dev->spi = NULL;
    return err;
}

mcp2515_err_t mcp2515_set_mode(mcp2515_t *dev, mcp2515_mode_t mode)
{
    if (!dev || !dev->spi) return MCP2515_ERR_SPI;

    bool leaving_sleep = dev->mode == MCP2515_MODE_SLEEP &&
                         mode != MCP2515_MODE_SLEEP;
    uint8_t inte = 0;

    if (leaving_sleep) {
        if (!dev->spi->delay_us) return MCP2515_ERR_PARAM;
        inte = wake_start(dev->spi);
    }

    uint8_t opmod = mode_to_opmod(mode);
    bit_modify(dev->spi, REG_CANCTRL, CANCTRL_REQOP_MASK, opmod);

    mcp2515_err_t err = wait_mode(dev->spi, opmod);
    if (leaving_sleep)
        wake_finish(dev->spi, inte);
    if (err == MCP2515_OK)
        dev->mode = mode;
    return err;
}

mcp2515_err_t mcp2515_send(mcp2515_t *dev, const mcp2515_msg_t *msg)
{
    if (!dev || !dev->spi || !msg) return MCP2515_ERR_SPI;
    if (msg->dlc > 8) return MCP2515_ERR_PARAM;

    /* DS20001801J p.60: Normal "is also the only mode in which the MCP2515 will
     * transmit messages over the CAN bus", and Loopback passes frames on inside
     * the chip. Elsewhere the frame stays pending, which by p.59 holds up every
     * later mode change. */
    if (dev->mode != MCP2515_MODE_NORMAL && dev->mode != MCP2515_MODE_LOOPBACK)
        return MCP2515_ERR_PARAM;

    /* check if TX buffer 0 is free */
    uint8_t ctrl = reg_read(dev->spi, REG_TXB0CTRL);
    if (ctrl & TXB_TXREQ) return MCP2515_ERR_TX_BUSY;

    /* write ID */
    encode_id(dev->spi, REG_TXB0SIDH, msg->id, msg->extended);

    /* write DLC (with RTR bit) */
    uint8_t dlc = msg->dlc;
    if (msg->rtr) dlc |= 0x40;
    reg_write(dev->spi, REG_TXB0DLC, dlc);

    /* write data */
    for (uint8_t i = 0; i < msg->dlc; i++)
        reg_write(dev->spi, (uint8_t)(REG_TXB0D0 + i), msg->data[i]);

    /* request to send */
    bit_modify(dev->spi, REG_TXB0CTRL, TXB_TXREQ, TXB_TXREQ);

    return MCP2515_OK;
}

mcp2515_err_t mcp2515_abort_tx(mcp2515_t *dev)
{
    if (!dev || !dev->spi) return MCP2515_ERR_SPI;

    /* p.16: clearing TXREQ aborts this buffer, and needs none of the undoing of
     * ABAT, which would reach all three */
    bit_modify(dev->spi, REG_TXB0CTRL, TXB_TXREQ, 0x00);

    if (reg_read(dev->spi, REG_TXB0CTRL) & TXB_TXREQ) return MCP2515_ERR_TX_BUSY;
    return MCP2515_OK;
}

mcp2515_err_t mcp2515_receive(mcp2515_t *dev, mcp2515_msg_t *msg)
{
    if (!dev || !dev->spi || !msg) return MCP2515_ERR_SPI;

    uint8_t intf = reg_read(dev->spi, REG_CANINTF);
    uint8_t sidh_base;
    uint8_t dlc_reg;
    uint8_t d0_reg;
    uint8_t rx_flag;

    if (intf & CANINTF_RX0IF) {
        sidh_base = REG_RXB0SIDH;
        dlc_reg   = REG_RXB0DLC;
        d0_reg    = REG_RXB0D0;
        rx_flag   = CANINTF_RX0IF;
    } else if (intf & CANINTF_RX1IF) {
        sidh_base = REG_RXB1SIDH;
        dlc_reg   = REG_RXB1DLC;
        d0_reg    = REG_RXB1D0;
        rx_flag   = CANINTF_RX1IF;
    } else {
        return MCP2515_ERR_NO_MSG;
    }

    /* decode ID */
    decode_id(dev->spi, sidh_base, &msg->id, &msg->extended);

    /* read DLC */
    uint8_t raw_dlc = reg_read(dev->spi, dlc_reg);
    uint8_t sidl = reg_read(dev->spi, (uint8_t)(sidh_base + 1));
    /* RTR in RXBnDLC is valid only for extended frames; SRR marks standard ones */
    msg->rtr = msg->extended ? (raw_dlc & 0x40) != 0 : (sidl & RXB_SRR) != 0;
    msg->dlc = raw_dlc & 0x0F;
    if (msg->dlc > 8) msg->dlc = 8;

    /* read data: a remote frame has none */
    for (uint8_t i = 0; !msg->rtr && i < msg->dlc; i++)
        msg->data[i] = reg_read(dev->spi, (uint8_t)(d0_reg + i));

    /* clear RX flag */
    bit_modify(dev->spi, REG_CANINTF, rx_flag, 0x00);

    return MCP2515_OK;
}

bool mcp2515_has_message(mcp2515_t *dev)
{
    if (!dev || !dev->spi) return false;
    uint8_t intf = reg_read(dev->spi, REG_CANINTF);
    return (intf & (CANINTF_RX0IF | CANINTF_RX1IF)) != 0;
}

mcp2515_err_t mcp2515_set_filter(mcp2515_t *dev, uint8_t filter_num,
                                  uint32_t id, bool extended)
{
    if (!dev || !dev->spi) return MCP2515_ERR_SPI;
    if (filter_num >= FILTER_COUNT) return MCP2515_ERR_PARAM;
    if (dev->mode != MCP2515_MODE_CONFIG) return MCP2515_ERR_PARAM;

    encode_id(dev->spi, filter_base[filter_num], id, extended);
    return MCP2515_OK;
}

mcp2515_err_t mcp2515_set_mask(mcp2515_t *dev, uint8_t mask_num,
                                uint32_t mask, bool extended)
{
    if (!dev || !dev->spi) return MCP2515_ERR_SPI;
    if (mask_num > 1) return MCP2515_ERR_PARAM;
    if (dev->mode != MCP2515_MODE_CONFIG) return MCP2515_ERR_PARAM;

    uint8_t base = (mask_num == 0) ? REG_RXM0SIDH : REG_RXM1SIDH;
    /* Reuse encode_id to write SID+EID mask bits consistently with set_filter */
    encode_id(dev->spi, base, mask, extended);
    return MCP2515_OK;
}

uint8_t mcp2515_read_status(mcp2515_t *dev)
{
    if (!dev || !dev->spi) return 0;
    return read_status_cmd(dev->spi);
}

uint8_t mcp2515_read_errors(mcp2515_t *dev)
{
    if (!dev || !dev->spi) return 0;
    return reg_read(dev->spi, REG_EFLG);
}
