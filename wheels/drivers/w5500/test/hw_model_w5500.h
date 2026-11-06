// SPDX-License-Identifier: MIT
/*
 * hw_model_w5500.h -- W5500 SPI Ethernet controller hardware model
 *
 * Models Wiznet W5500 (Datasheet Version 1.1.0) SPI register + buffer behavior.
 * Page numbers are those of that PDF.
 * - SPI frame: [offset_hi, offset_lo, control, data...]
 *   control byte: BSB[4:0] << 3 | RWB << 2 | OM[1:0]
 *   BSB=0: common registers; BSB=1..31: per-socket reg/TX/RX blocks
 *   RWB: 0=read, 1=write; OM=00: variable-length data mode
 * - Common register block, with the documented reset values
 * - 8 sockets, each with reg block + 2KB TX buffer + 2KB RX buffer. The driver
 *   keeps the reset size, so the model does not resize them. Any 16-bit offset
 *   reaches them, and offsets alias every 2 KB.
 * - Sn_CR clears when the chip accepts a command, and the command goes on from
 *   there: Sn_SR moves and Sn_IR sets some reads later
 * - OPEN initialises the pointers, SEND moves Sn_TX_RD up to Sn_TX_WR, RECV
 *   tells the chip the new Sn_RX_RD
 * - Sn_TX_FSR and Sn_RX_RSR follow the pointers
 * - Sn_IR is RCW1 and gated by Sn_IMR, [R] registers ignore writes
 * - Software reset via MR bit 7 (RST)
 *
 * Header-only. Include from test files.
 */

#ifndef HW_MODEL_W5500_H
#define HW_MODEL_W5500_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* --- SPI control byte fields --- */
#define W5_CTRL_WRITE   0x04u
#define W5_CTRL_READ    0x00u
/* OM = 00 (variable-length) is default (low 2 bits = 0) */

/* --- Block Select Bits (BSB) --- */
#define W5_BSB_COMMON       0x00u
#define W5_BSB_SOCK_REG(n)  ((uint8_t)((n) * 4u + 1u))
#define W5_BSB_SOCK_TX(n)   ((uint8_t)((n) * 4u + 2u))
#define W5_BSB_SOCK_RX(n)   ((uint8_t)((n) * 4u + 3u))

/* --- Common register addresses --- */
#define W5_REG_MR        0x0000u
#define W5_REG_GAR0      0x0001u
#define W5_REG_SUBR0     0x0005u
#define W5_REG_SHAR0     0x0009u
#define W5_REG_SIPR0     0x000Fu
#define W5_REG_RTR0      0x0019u
#define W5_REG_RTR1      0x001Au
#define W5_REG_RCR       0x001Bu
#define W5_REG_PTIMER    0x001Cu
#define W5_REG_PMRU0     0x0026u
#define W5_REG_PMRU1     0x0027u
#define W5_REG_UIPR0     0x0028u
#define W5_REG_UPORTR1   0x002Du
#define W5_REG_PHYCFGR   0x002Eu
#define W5_REG_VERSIONR  0x0039u

#define W5_MR_RST        0x80u

/* --- Socket register offsets (within each socket block) --- */
#define W5_SN_MR          0x0000u
#define W5_SN_CR          0x0001u
#define W5_SN_IR          0x0002u
#define W5_SN_SR          0x0003u
#define W5_SN_PORT0       0x0004u
#define W5_SN_PORT1       0x0005u
#define W5_SN_DHAR0       0x0006u
#define W5_SN_TTL         0x0016u
#define W5_SN_TXBUF_SIZE  0x001Fu
#define W5_SN_RXBUF_SIZE  0x001Eu
#define W5_SN_TX_FSR0     0x0020u
#define W5_SN_TX_FSR1     0x0021u
#define W5_SN_TX_RD0      0x0022u
#define W5_SN_TX_RD1      0x0023u
#define W5_SN_TX_WR0      0x0024u
#define W5_SN_TX_WR1      0x0025u
#define W5_SN_RX_RSR0     0x0026u
#define W5_SN_RX_RSR1     0x0027u
#define W5_SN_RX_RD0      0x0028u
#define W5_SN_RX_RD1      0x0029u
#define W5_SN_RX_WR0      0x002Au
#define W5_SN_RX_WR1      0x002Bu
#define W5_SN_IMR         0x002Cu
#define W5_SN_FRAG0       0x002Du

/* --- Socket commands --- */
#define W5_CMD_OPEN    0x01u
#define W5_CMD_CLOSE   0x10u
#define W5_CMD_SEND    0x20u
#define W5_CMD_RECV    0x40u

/* --- Sn_IR bits (p.48) --- */
#define W5_IR_SEND_OK  0x10u
#define W5_IR_TIMEOUT  0x08u
#define W5_IR_RECV     0x04u

/* --- Socket status values --- */
#define W5_SOCK_CLOSED       0x00u
#define W5_SOCK_INIT         0x13u
#define W5_SOCK_UDP          0x22u
#define W5_SOCK_MACRAW       0x42u

/* --- Socket protocol modes --- */
#define W5_PROTO_TCP     0x01u
#define W5_PROTO_UDP     0x02u
#define W5_PROTO_MACRAW  0x04u
#define W5_PROTO_MASK    0x0Fu

/* --- Buffer sizes --- */
#define W5_MAX_SOCKETS       8u
#define W5_COMMON_REG_SIZE   0x0040u   /* common register space */
#define W5_SOCK_REG_SIZE     0x0030u   /* per-socket register space */
#define W5_SOCK_BUF_SIZE     2048u     /* 2KB per socket TX/RX */

/* --- SPI transaction phases --- */
typedef enum {
    W5_PH_ADDR_HI = 0,   /* waiting for offset high byte  */
    W5_PH_ADDR_LO,       /* waiting for offset low byte   */
    W5_PH_CTRL,          /* waiting for control byte       */
    W5_PH_DATA,          /* transferring data bytes        */
} w5500_phase_t;

/* --- The hardware model --- */
typedef struct {
    /* Common registers */
    uint8_t       common_regs[W5_COMMON_REG_SIZE];
    /* Per-socket registers (8 sockets) */
    uint8_t       sock_regs[W5_MAX_SOCKETS][W5_SOCK_REG_SIZE];
    /* Per-socket TX and RX buffers */
    uint8_t       tx_buf[W5_MAX_SOCKETS][W5_SOCK_BUF_SIZE];
    uint8_t       rx_buf[W5_MAX_SOCKETS][W5_SOCK_BUF_SIZE];

    /* SPI transaction state */
    w5500_phase_t phase;
    uint16_t      spi_addr;     /* 16-bit offset from address phase */
    uint8_t       spi_bsb;     /* block select bits (BSB) */
    bool          spi_write;   /* true = write, false = read */
    uint16_t      data_idx;    /* byte index within data phase */

    /* Counters */
    int           reset_count;
    int           delay_count;

    /* Reads of Sn_CR that still see the command, 0 = accepted at once */
    int           cr_lag;
    int           cr_left[W5_MAX_SOCKETS];
    /* Reads of Sn_SR that still see the old status after Sn_CR has cleared */
    int           sr_lag;
    int           sr_left[W5_MAX_SOCKETS];
    bool          sr_pending[W5_MAX_SOCKETS];
    uint8_t       sr_next[W5_MAX_SOCKETS];
    /* Reads of Sn_IR before a SEND ends, and what it ends with: W5_IR_SEND_OK,
     * W5_IR_TIMEOUT, or 0 for a SEND that never ends */
    int           ir_lag;
    int           ir_left[W5_MAX_SOCKETS];
    bool          send_pending[W5_MAX_SOCKETS];
    uint8_t       send_result;

    /* The Sn_RX_RD the chip knows: the one the last RECV told it */
    uint16_t      rx_rd_chip[W5_MAX_SOCKETS];

    /* What the last SEND took from the TX buffer */
    uint8_t       sent[W5_MAX_SOCKETS][W5_SOCK_BUF_SIZE];
    uint16_t      sent_len[W5_MAX_SOCKETS];

    /* Faults a correct driver does not commit */
    int           open_not_closed;   /* OPEN with Sn_SR not at SOCK_CLOSED */
    int           macraw_not_sock0;  /* OPEN of MACRAW on a socket but 0 */
    int           send_while_busy;   /* SEND with the last one unfinished */
    int           tx_overruns;       /* SEND of more than the buffer holds */
    int           empty_frames;      /* SPI frames with no data phase */
} w5500_model_t;

/* ================================================================
 * Internal helpers
 * ================================================================ */

/* Identify which block a BSB value refers to and return socket index.
 * Returns -1 for common block. */
__attribute__((unused))
static int w5_bsb_socket(uint8_t bsb)
{
    if (bsb == 0) return -1;
    return (int)((bsb - 1) / 4);
}

/* Identify block type: 0=common, 1=sock_reg, 2=sock_tx, 3=sock_rx */
__attribute__((unused))
static int w5_bsb_type(uint8_t bsb)
{
    if (bsb == 0) return 0;
    return (int)((bsb - 1) % 4);  /* 0=reg, 1=tx, 2=rx */
}

__attribute__((unused))
static uint16_t w5_get16(const w5500_model_t *m, uint8_t sock, uint16_t addr)
{
    return (uint16_t)(((uint16_t)m->sock_regs[sock][addr] << 8) |
                      m->sock_regs[sock][addr + 1u]);
}

__attribute__((unused))
static void w5_set16(w5500_model_t *m, uint8_t sock, uint16_t addr, uint16_t val)
{
    m->sock_regs[sock][addr]      = (uint8_t)(val >> 8);
    m->sock_regs[sock][addr + 1u] = (uint8_t)(val & 0xFFu);
}

/* p.54: Sn_TX_FSR is "the difference between 'Socket n TX Write Pointer
 * (Sn_TX_WR)' and 'Socket n TX Read Pointer (Sn_TX_RD)'", out of the buffer */
__attribute__((unused))
static void w5_update_tx_fsr(w5500_model_t *m, uint8_t sock)
{
    uint16_t used = (uint16_t)(w5_get16(m, sock, W5_SN_TX_WR0) -
                               w5_get16(m, sock, W5_SN_TX_RD0));
    w5_set16(m, sock, W5_SN_TX_FSR0, used < W5_SOCK_BUF_SIZE
             ? (uint16_t)(W5_SOCK_BUF_SIZE - used) : 0u);
}

/* p.55: Sn_RX_RSR "is calculated as the difference between 'Socket n RX Write
 * Pointer (Sn_RX_WR)' and 'Socket n RX Read Pointer (Sn_RX_RD)'" */
__attribute__((unused))
static void w5_update_rx_rsr(w5500_model_t *m, uint8_t sock)
{
    w5_set16(m, sock, W5_SN_RX_RSR0,
             (uint16_t)(w5_get16(m, sock, W5_SN_RX_WR0) -
                        m->rx_rd_chip[sock]));
}

/* p.48: "When an interrupt occurs and the corresponding bit of Sn_IMR is '1',
 * the corresponding bit of Sn_IR becomes '1'." */
__attribute__((unused))
static void w5_raise_ir(w5500_model_t *m, uint8_t sock, uint8_t bits)
{
    m->sock_regs[sock][W5_SN_IR] |=
        (uint8_t)(bits & m->sock_regs[sock][W5_SN_IMR]);
}

/* A status change that shows only after sr_lag more reads of Sn_SR */
__attribute__((unused))
static void w5_set_status(w5500_model_t *m, uint8_t sock, uint8_t status)
{
    if (m->sr_lag > 0 && status != m->sock_regs[sock][W5_SN_SR]) {
        m->sr_next[sock]    = status;
        m->sr_left[sock]    = m->sr_lag;
        m->sr_pending[sock] = true;
    } else {
        m->sock_regs[sock][W5_SN_SR] = status;
        m->sr_pending[sock] = false;
    }
}

/* The end of a SEND */
__attribute__((unused))
static void w5_send_end(w5500_model_t *m, uint8_t sock)
{
    m->send_pending[sock] = false;
    if (m->send_result == W5_IR_TIMEOUT) {
        /* p.49: "when a timeout occurs, it is changed to SOCK_CLOSED
         * regardless of previous status" */
        m->sock_regs[sock][W5_SN_SR] = W5_SOCK_CLOSED;
    } else {
        /* p.54: "After transmitting the saved data, the SEND command increases
         * the Sn_TX_RD as same as the Sn_TX_WR." */
        w5_set16(m, sock, W5_SN_TX_RD0, w5_get16(m, sock, W5_SN_TX_WR0));
        w5_update_tx_fsr(m, sock);
    }
    w5_raise_ir(m, sock, m->send_result);
}

/* p.54: "SEND command transmits the saved data from the current Sn_TX_RD to
 * the Sn_TX_WR in the Socket n TX Buffer." */
__attribute__((unused))
static void w5_send_start(w5500_model_t *m, uint8_t sock)
{
    uint16_t rd  = w5_get16(m, sock, W5_SN_TX_RD0);
    uint16_t len = (uint16_t)(w5_get16(m, sock, W5_SN_TX_WR0) - rd);

    if (m->send_pending[sock])
        m->send_while_busy++;
    if (len > W5_SOCK_BUF_SIZE) {
        m->tx_overruns++;
        len = W5_SOCK_BUF_SIZE;
    }
    for (uint16_t i = 0; i < len; i++) {
        m->sent[sock][i] =
            m->tx_buf[sock][(uint16_t)(rd + i) % W5_SOCK_BUF_SIZE];
    }
    m->sent_len[sock] = len;

    m->send_pending[sock] = true;
    m->ir_left[sock] = m->ir_lag;
    if (m->send_result != 0u && m->ir_lag == 0)
        w5_send_end(m, sock);
}

/* Sn_CR has cleared: what the accepted command does */
__attribute__((unused))
static void w5_exec_socket_cmd(w5500_model_t *m, uint8_t sock, uint8_t cmd)
{
    if (sock >= W5_MAX_SOCKETS) return;

    uint8_t proto = m->sock_regs[sock][W5_SN_MR] & W5_PROTO_MASK;

    switch (cmd) {
    case W5_CMD_OPEN:
        if (m->sock_regs[sock][W5_SN_SR] != W5_SOCK_CLOSED ||
            m->sr_pending[sock])
            m->open_not_closed++;
        /* p.54 to p.56: all four pointers are "initialized by OPEN command" */
        w5_set16(m, sock, W5_SN_TX_RD0, 0u);
        w5_set16(m, sock, W5_SN_TX_WR0, 0u);
        w5_set16(m, sock, W5_SN_RX_RD0, 0u);
        w5_set16(m, sock, W5_SN_RX_WR0, 0u);
        m->rx_rd_chip[sock] = 0u;
        w5_update_tx_fsr(m, sock);
        w5_update_rx_rsr(m, sock);
        if (proto == W5_PROTO_TCP)
            w5_set_status(m, sock, W5_SOCK_INIT);
        else if (proto == W5_PROTO_UDP)
            w5_set_status(m, sock, W5_SOCK_UDP);
        else if (proto == W5_PROTO_MACRAW && sock == 0u)
            w5_set_status(m, sock, W5_SOCK_MACRAW);
        else if (proto == W5_PROTO_MACRAW)
            m->macraw_not_sock0++;   /* p.45: "only used in Socket 0" */
        break;
    case W5_CMD_CLOSE:
        m->send_pending[sock] = false;
        w5_set_status(m, sock, W5_SOCK_CLOSED);
        break;
    case W5_CMD_SEND:
        w5_send_start(m, sock);
        break;
    case W5_CMD_RECV:
        /* p.55: RECV "is for notifying the updated Sn_RX_RD to W5500" */
        m->rx_rd_chip[sock] = w5_get16(m, sock, W5_SN_RX_RD0);
        w5_update_rx_rsr(m, sock);
        break;
    default:
        break;
    }
}

/* The reset values on pages 32 to 43 and 44 to 57 that are not 0 */
__attribute__((unused))
static void w5_load_reset_values(w5500_model_t *m)
{
    memset(m->common_regs, 0, sizeof(m->common_regs));
    m->common_regs[W5_REG_RTR0]     = 0x07u;   /* 0x07D0 */
    m->common_regs[W5_REG_RTR1]     = 0xD0u;
    m->common_regs[W5_REG_RCR]      = 0x08u;
    m->common_regs[W5_REG_PTIMER]   = 0x28u;
    m->common_regs[W5_REG_PMRU0]    = 0xFFu;   /* 0xFFFF */
    m->common_regs[W5_REG_PMRU1]    = 0xFFu;
    m->common_regs[W5_REG_PHYCFGR]  = 0xB8u;   /* 0b10111XXX */
    m->common_regs[W5_REG_VERSIONR] = 0x04u;

    for (uint8_t s = 0; s < W5_MAX_SOCKETS; s++) {
        memset(m->sock_regs[s], 0, sizeof(m->sock_regs[s]));
        memset(&m->sock_regs[s][W5_SN_DHAR0], 0xFF, 6);
        m->sock_regs[s][W5_SN_TTL]         = 0x80u;
        m->sock_regs[s][W5_SN_RXBUF_SIZE]  = 0x02u;
        m->sock_regs[s][W5_SN_TXBUF_SIZE]  = 0x02u;
        m->sock_regs[s][W5_SN_TX_FSR0]     = 0x08u;   /* 0x0800 */
        m->sock_regs[s][W5_SN_IMR]         = 0xFFu;
        m->sock_regs[s][W5_SN_FRAG0]       = 0x40u;   /* 0x4000 */

        m->cr_left[s]      = 0;
        m->sr_left[s]      = 0;
        m->sr_pending[s]   = false;
        m->ir_left[s]      = 0;
        m->send_pending[s] = false;
        m->rx_rd_chip[s]   = 0u;
    }
}

/* Software reset: p.32, "All internal registers will be initialized" */
__attribute__((unused))
static void w5_soft_reset(w5500_model_t *m)
{
    w5_load_reset_values(m);
    m->reset_count++;
}

/* [R] common registers: UIPR and UPORTR (p.41), VERSIONR (p.43) */
__attribute__((unused))
static bool w5_common_read_only(uint16_t addr)
{
    return addr == W5_REG_VERSIONR ||
           (addr >= W5_REG_UIPR0 && addr <= W5_REG_UPORTR1);
}

/* [R] socket registers: Sn_SR (p.49), Sn_TX_FSR (p.53), Sn_TX_RD (p.54),
 * Sn_RX_RSR and Sn_RX_WR (p.55) */
__attribute__((unused))
static bool w5_sock_read_only(uint16_t addr)
{
    return addr == W5_SN_SR ||
           (addr >= W5_SN_TX_FSR0 && addr <= W5_SN_TX_RD1) ||
           addr == W5_SN_RX_RSR0 || addr == W5_SN_RX_RSR1 ||
           addr == W5_SN_RX_WR0 || addr == W5_SN_RX_WR1;
}

__attribute__((unused))
static void w5_write_sock_reg(w5500_model_t *m, uint8_t sock, uint16_t addr,
                              uint8_t val)
{
    if (w5_sock_read_only(addr))
        return;

    /* p.32 note 2: RCW1, "clear this bit by writing 1. Writing '0' has no
     * effect on the bit value." */
    if (addr == W5_SN_IR) {
        m->sock_regs[sock][W5_SN_IR] &= (uint8_t)~val;
        return;
    }

    m->sock_regs[sock][addr] = val;

    /* p.46: "After W5500 accepts the command, the Sn_CR register is
     * automatically cleared to 0x00." */
    if (addr == W5_SN_CR && val != 0u) {
        m->cr_left[sock] = m->cr_lag;
        if (m->cr_lag == 0) {
            m->sock_regs[sock][W5_SN_CR] = 0x00u;
            w5_exec_socket_cmd(m, sock, val);
        }
    }
}

__attribute__((unused))
static uint8_t w5_read_sock_reg(w5500_model_t *m, uint8_t sock, uint16_t addr)
{
    uint8_t *r = m->sock_regs[sock];

    if (addr == W5_SN_CR && r[W5_SN_CR] != 0u) {
        if (m->cr_left[sock] > 0) {
            m->cr_left[sock]--;
        } else {
            uint8_t cmd = r[W5_SN_CR];
            r[W5_SN_CR] = 0x00u;
            w5_exec_socket_cmd(m, sock, cmd);
        }
    } else if (addr == W5_SN_SR && m->sr_pending[sock]) {
        if (m->sr_left[sock] > 0) {
            m->sr_left[sock]--;
        } else {
            r[W5_SN_SR] = m->sr_next[sock];
            m->sr_pending[sock] = false;
        }
    } else if (addr == W5_SN_IR && m->send_pending[sock] &&
               m->send_result != 0u) {
        if (m->ir_left[sock] > 0)
            m->ir_left[sock]--;
        else
            w5_send_end(m, sock);
    } else if (addr == W5_SN_TX_FSR0 || addr == W5_SN_TX_FSR1) {
        w5_update_tx_fsr(m, sock);
    }
    return r[addr];
}

/* ================================================================
 * SPI HAL callbacks
 * ================================================================ */

__attribute__((unused))
static void w5500_model_cs_low(void *ctx)
{
    w5500_model_t *m = (w5500_model_t *)ctx;
    m->phase = W5_PH_ADDR_HI;
    m->spi_addr = 0;
    m->spi_bsb = 0;
    m->spi_write = false;
    m->data_idx = 0;
}

__attribute__((unused))
static void w5500_model_cs_high(void *ctx)
{
    w5500_model_t *m = (w5500_model_t *)ctx;
    /* p.17: "Variable Data Length Mode, N-Bytes Data Phase (1 <= N)" */
    if (m->phase != W5_PH_ADDR_HI && m->data_idx == 0)
        m->empty_frames++;
    m->phase = W5_PH_ADDR_HI;
}

__attribute__((unused))
static uint8_t w5500_model_xfer(void *ctx, uint8_t tx)
{
    w5500_model_t *m = (w5500_model_t *)ctx;

    switch (m->phase) {
    case W5_PH_ADDR_HI:
        m->spi_addr = (uint16_t)((uint16_t)tx << 8);
        m->phase = W5_PH_ADDR_LO;
        return 0xFFu;

    case W5_PH_ADDR_LO:
        m->spi_addr |= tx;
        m->phase = W5_PH_CTRL;
        return 0xFFu;

    case W5_PH_CTRL: {
        m->spi_bsb = (uint8_t)((tx >> 3) & 0x1Fu);
        m->spi_write = (tx & W5_CTRL_WRITE) != 0;
        m->data_idx = 0;
        m->phase = W5_PH_DATA;

        /* Check for MR reset on write to common block */
        /* (handled in data phase when writing MR) */
        return 0xFFu;
    }

    case W5_PH_DATA: {
        uint16_t addr = (uint16_t)(m->spi_addr + m->data_idx);
        m->data_idx++;

        int bsb_type = w5_bsb_type(m->spi_bsb);
        int sock = w5_bsb_socket(m->spi_bsb);
        /* p.27: "it can be accessible within the 16 bits offset address range
         * (From 0x0000 to 0xFFFF)", and p.31: the offset "automatically
         * converts into the physical address" */
        uint16_t buf_at = (uint16_t)(addr % W5_SOCK_BUF_SIZE);

        if (m->spi_write) {
            /* --- WRITE --- */
            if (m->spi_bsb == 0) {
                /* Common register block */
                if (addr < W5_COMMON_REG_SIZE) {
                    if (!w5_common_read_only(addr))
                        m->common_regs[addr] = tx;
                    /* Check for software reset */
                    if (addr == W5_REG_MR && (tx & W5_MR_RST))
                        w5_soft_reset(m);
                }
            } else if (sock >= 0 && sock < (int)W5_MAX_SOCKETS) {
                if (bsb_type == 0) {
                    /* Socket register block */
                    if (addr < W5_SOCK_REG_SIZE)
                        w5_write_sock_reg(m, (uint8_t)sock, addr, tx);
                } else if (bsb_type == 1) {
                    /* Socket TX buffer */
                    m->tx_buf[sock][buf_at] = tx;
                } else if (bsb_type == 2) {
                    /* Socket RX buffer (normally read-only, but allow
                     * test injection) */
                    m->rx_buf[sock][buf_at] = tx;
                }
            }
            return 0xFFu;
        } else {
            /* --- READ --- */
            if (m->spi_bsb == 0) {
                if (addr < W5_COMMON_REG_SIZE)
                    return m->common_regs[addr];
            } else if (sock >= 0 && sock < (int)W5_MAX_SOCKETS) {
                if (bsb_type == 0) {
                    if (addr < W5_SOCK_REG_SIZE)
                        return w5_read_sock_reg(m, (uint8_t)sock, addr);
                } else if (bsb_type == 1) {
                    return m->tx_buf[sock][buf_at];
                } else if (bsb_type == 2) {
                    return m->rx_buf[sock][buf_at];
                }
            }
            return 0xFFu;
        }
    }
    } /* switch */

    return 0xFFu;
}

__attribute__((unused))
static void w5500_model_delay_ms(void *ctx, uint32_t ms)
{
    (void)ms;
    w5500_model_t *m = (w5500_model_t *)ctx;
    m->delay_count++;
}

/* ================================================================
 * Model initialization
 * ================================================================ */

__attribute__((unused))
static void w5500_model_init(w5500_model_t *m)
{
    memset(m, 0, sizeof(*m));

    w5_load_reset_values(m);

    /* Initial SPI state */
    m->phase = W5_PH_ADDR_HI;
    m->reset_count = 0;
    m->delay_count = 0;

    /* A driver has to wait at each step, so none of them is instant */
    m->cr_lag = 1;
    m->sr_lag = 2;
    m->ir_lag = 2;
    m->send_result = W5_IR_SEND_OK;
}

/* ================================================================
 * Helper: data arriving from the network
 *
 * Stores data at Sn_RX_WR, advances it and updates Sn_RX_RSR, which "does not
 * exceed the Sn_RXBUF_SIZE" (p.55), so what does not fit is dropped.
 * ================================================================ */

__attribute__((unused))
static void w5500_model_inject_rx(w5500_model_t *m, uint8_t sock,
                                  const uint8_t *data, uint16_t len)
{
    if (sock >= W5_MAX_SOCKETS) return;

    uint16_t wr   = w5_get16(m, sock, W5_SN_RX_WR0);
    uint16_t used = (uint16_t)(wr - m->rx_rd_chip[sock]);
    uint16_t room = (uint16_t)(W5_SOCK_BUF_SIZE - used);
    if (len > room) len = room;

    for (uint16_t i = 0; i < len; i++) {
        m->rx_buf[sock][(uint16_t)(wr + i) % W5_SOCK_BUF_SIZE] = data[i];
    }

    /* p.56: Sn_RX_WR "is auto-increased by the data reception" */
    w5_set16(m, sock, W5_SN_RX_WR0, (uint16_t)(wr + len));
    w5_update_rx_rsr(m, sock);
    w5_raise_ir(m, sock, W5_IR_RECV);
}

/* Helper: where a socket that has moved a lot of data would have its pointers */
__attribute__((unused))
static void w5500_model_set_rx_pointer(w5500_model_t *m, uint8_t sock,
                                       uint16_t ptr)
{
    if (sock >= W5_MAX_SOCKETS) return;
    w5_set16(m, sock, W5_SN_RX_RD0, ptr);
    w5_set16(m, sock, W5_SN_RX_WR0, ptr);
    m->rx_rd_chip[sock] = ptr;
    w5_update_rx_rsr(m, sock);
}

/* Helper: set MAC address in common registers (for verification tests) */
__attribute__((unused))
static void w5500_model_get_mac(const w5500_model_t *m, uint8_t mac[6])
{
    for (unsigned i = 0; i < 6u; i++)
        mac[i] = m->common_regs[W5_REG_SHAR0 + i];
}

/* Helper: get socket status */
__attribute__((unused))
static uint8_t w5500_model_get_sock_status(const w5500_model_t *m, uint8_t sock)
{
    if (sock >= W5_MAX_SOCKETS) return 0xFF;
    return m->sock_regs[sock][W5_SN_SR];
}

#endif /* HW_MODEL_W5500_H */
