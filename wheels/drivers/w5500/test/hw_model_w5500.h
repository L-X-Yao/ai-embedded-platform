// SPDX-License-Identifier: MIT
/*
 * hw_model_w5500.h -- W5500 SPI Ethernet controller hardware model
 *
 * Models Wiznet W5500 (datasheet v1.0.9) SPI register + buffer behavior:
 * - SPI frame: [offset_hi, offset_lo, control, data...]
 *   control byte: BSB[4:0] << 3 | RWB << 2 | OM[1:0]
 *   BSB=0: common registers; BSB=1..32: per-socket reg/TX/RX blocks
 *   RWB: 0=read, 1=write; OM=00: variable-length data mode
 * - Common register block: MR (0x0000), GAR (0x0001-0x0004),
 *   SUBR (0x0005-0x0008), SHAR (0x0009-0x000E), SIPR (0x000F-0x0012),
 *   VERSIONR (0x0039, read-only, always 0x04)
 * - 8 sockets, each with reg block + 2KB TX buffer + 2KB RX buffer
 * - Socket registers: Sn_MR, Sn_CR, Sn_IR, Sn_SR, Sn_PORT, etc.
 * - Sn_CR (command register): auto-clears to 0x00 after execution
 * - Sn_SR (status register): set by command execution
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
#define W5_REG_VERSIONR  0x0039u

#define W5_MR_RST        0x80u

/* --- Socket register offsets (within each socket block) --- */
#define W5_SN_MR          0x0000u
#define W5_SN_CR          0x0001u
#define W5_SN_IR          0x0002u
#define W5_SN_SR          0x0003u
#define W5_SN_PORT0       0x0004u
#define W5_SN_PORT1       0x0005u
#define W5_SN_TXBUF_SIZE  0x001Fu
#define W5_SN_RXBUF_SIZE  0x001Eu
#define W5_SN_TX_FSR0     0x0020u
#define W5_SN_TX_FSR1     0x0021u
#define W5_SN_TX_WR0      0x0024u
#define W5_SN_TX_WR1      0x0025u
#define W5_SN_RX_RSR0     0x0026u
#define W5_SN_RX_RSR1     0x0027u
#define W5_SN_RX_RD0      0x0028u
#define W5_SN_RX_RD1      0x0029u

/* --- Socket commands --- */
#define W5_CMD_OPEN    0x01u
#define W5_CMD_CLOSE   0x10u
#define W5_CMD_SEND    0x20u
#define W5_CMD_RECV    0x40u

/* --- Socket status values --- */
#define W5_SOCK_CLOSED       0x00u
#define W5_SOCK_INIT         0x13u
#define W5_SOCK_UDP          0x22u
#define W5_SOCK_MACRAW       0x42u

/* --- Socket protocol modes --- */
#define W5_PROTO_TCP     0x01u
#define W5_PROTO_UDP     0x02u
#define W5_PROTO_MACRAW  0x04u

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

/* Execute socket command (written to Sn_CR) */
__attribute__((unused))
static void w5_exec_socket_cmd(w5500_model_t *m, uint8_t sock, uint8_t cmd)
{
    if (sock >= W5_MAX_SOCKETS) return;

    uint8_t proto = m->sock_regs[sock][W5_SN_MR];

    switch (cmd) {
    case W5_CMD_OPEN:
        if (proto == W5_PROTO_TCP)
            m->sock_regs[sock][W5_SN_SR] = W5_SOCK_INIT;
        else if (proto == W5_PROTO_UDP)
            m->sock_regs[sock][W5_SN_SR] = W5_SOCK_UDP;
        else if (proto == W5_PROTO_MACRAW)
            m->sock_regs[sock][W5_SN_SR] = W5_SOCK_MACRAW;
        break;
    case W5_CMD_CLOSE:
        m->sock_regs[sock][W5_SN_SR] = W5_SOCK_CLOSED;
        break;
    case W5_CMD_SEND:
        /* Mark send complete (simplified) */
        break;
    case W5_CMD_RECV:
        /* Mark recv complete (simplified) */
        break;
    default:
        break;
    }

    /* Command register auto-clears to 0x00 */
    m->sock_regs[sock][W5_SN_CR] = 0x00u;
}

/* Software reset: clear registers to defaults */
__attribute__((unused))
static void w5_soft_reset(w5500_model_t *m)
{
    /* Preserve version register */
    memset(m->common_regs, 0, sizeof(m->common_regs));
    m->common_regs[W5_REG_VERSIONR] = 0x04u;
    /* Clear MR reset bit */
    m->common_regs[W5_REG_MR] = 0x00u;

    /* Reset all socket registers */
    for (uint8_t s = 0; s < W5_MAX_SOCKETS; s++) {
        memset(m->sock_regs[s], 0, sizeof(m->sock_regs[s]));
        /* Default TX free size = 2048 (0x0800) */
        m->sock_regs[s][W5_SN_TX_FSR0] = 0x08u;
        m->sock_regs[s][W5_SN_TX_FSR1] = 0x00u;
        /* Default buffer sizes */
        m->sock_regs[s][W5_SN_TXBUF_SIZE] = 2u;
        m->sock_regs[s][W5_SN_RXBUF_SIZE] = 2u;
    }

    m->reset_count++;
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

        if (m->spi_write) {
            /* --- WRITE --- */
            if (m->spi_bsb == 0) {
                /* Common register block */
                if (addr < W5_COMMON_REG_SIZE) {
                    /* VERSIONR is read-only */
                    if (addr != W5_REG_VERSIONR)
                        m->common_regs[addr] = tx;
                    /* Check for software reset */
                    if (addr == W5_REG_MR && (tx & W5_MR_RST))
                        w5_soft_reset(m);
                }
            } else if (sock >= 0 && sock < (int)W5_MAX_SOCKETS) {
                if (bsb_type == 0) {
                    /* Socket register block */
                    if (addr < W5_SOCK_REG_SIZE) {
                        m->sock_regs[sock][addr] = tx;
                        /* Execute command when Sn_CR is written */
                        if (addr == W5_SN_CR && tx != 0)
                            w5_exec_socket_cmd(m, (uint8_t)sock, tx);
                    }
                } else if (bsb_type == 1) {
                    /* Socket TX buffer */
                    if (addr < W5_SOCK_BUF_SIZE)
                        m->tx_buf[sock][addr] = tx;
                } else if (bsb_type == 2) {
                    /* Socket RX buffer (normally read-only, but allow
                     * test injection) */
                    if (addr < W5_SOCK_BUF_SIZE)
                        m->rx_buf[sock][addr] = tx;
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
                        return m->sock_regs[sock][addr];
                } else if (bsb_type == 1) {
                    if (addr < W5_SOCK_BUF_SIZE)
                        return m->tx_buf[sock][addr];
                } else if (bsb_type == 2) {
                    if (addr < W5_SOCK_BUF_SIZE)
                        return m->rx_buf[sock][addr];
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

    /* VERSIONR = 0x04 (chip version, read-only) */
    m->common_regs[W5_REG_VERSIONR] = 0x04u;

    /* Default buffer configuration: 2KB per socket for TX and RX */
    for (uint8_t s = 0; s < W5_MAX_SOCKETS; s++) {
        m->sock_regs[s][W5_SN_TXBUF_SIZE] = 2u;
        m->sock_regs[s][W5_SN_RXBUF_SIZE] = 2u;
        /* TX free size = 2048 (0x0800) big-endian */
        m->sock_regs[s][W5_SN_TX_FSR0] = 0x08u;
        m->sock_regs[s][W5_SN_TX_FSR1] = 0x00u;
    }

    /* Clear buffers */
    memset(m->tx_buf, 0, sizeof(m->tx_buf));
    memset(m->rx_buf, 0, sizeof(m->rx_buf));

    /* Initial SPI state */
    m->phase = W5_PH_ADDR_HI;
    m->reset_count = 0;
    m->delay_count = 0;
}

/* ================================================================
 * Helper: inject RX data into a socket's RX buffer
 *
 * Writes data at the given offset and updates RX_RSR (received size).
 * ================================================================ */

__attribute__((unused))
static void w5500_model_inject_rx(w5500_model_t *m, uint8_t sock,
                                  const uint8_t *data, uint16_t len)
{
    if (sock >= W5_MAX_SOCKETS) return;
    if (len > W5_SOCK_BUF_SIZE) len = W5_SOCK_BUF_SIZE;

    /* Write data at RX read pointer offset */
    uint16_t rd = (uint16_t)(((uint16_t)m->sock_regs[sock][W5_SN_RX_RD0] << 8) |
                             m->sock_regs[sock][W5_SN_RX_RD1]);
    for (uint16_t i = 0; i < len; i++) {
        m->rx_buf[sock][(rd + i) % W5_SOCK_BUF_SIZE] = data[i];
    }

    /* Update RX received size (big-endian) */
    m->sock_regs[sock][W5_SN_RX_RSR0] = (uint8_t)(len >> 8);
    m->sock_regs[sock][W5_SN_RX_RSR1] = (uint8_t)(len & 0xFFu);
}

/* Helper: set MAC address in common registers (for verification tests) */
__attribute__((unused))
static void w5500_model_get_mac(const w5500_model_t *m, uint8_t mac[6])
{
    for (int i = 0; i < 6; i++)
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
