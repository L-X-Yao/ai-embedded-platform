// SPDX-License-Identifier: MIT
/*
 * hw_model_nrf24l01.h -- nRF24L01+ hardware register model
 *
 * Follows the nRF24L01+ Product Specification v1.0:
 * - SPI commands, STATUS on the first byte of every transaction (p51)
 * - Time advances through delay_us. Writes are ignored during the 100 ms
 *   power on reset (Figure 4, p22), CE must not rise before Tpd2stby (p24)
 * - W_REGISTER other than STATUS is ignored and counted outside power down
 *   and standby (p51)
 * - TX and RX FIFOs of three payloads (p56). R_RX_PAYLOAD pops, TX_DS pops,
 *   MAX_RT keeps the payload and blocks TX until it is cleared (p56, p59)
 * - TX needs PWR_UP, PRIM_RX low, a payload and CE high for 10 us (p23),
 *   and its result comes Tstby2a after that (p24)
 * - At 250 kbps with auto-ack an ARD below 500 us gets no ACK (p63 note a)
 * - STATUS flags clear on write 1, RX_P_NO and TX_FULL are read-only and
 *   follow the FIFOs (p59). OBSERVE_TX, RPD and FIFO_STATUS are read-only
 * - A packet lands only in RX mode on an enabled pipe of matching width
 *   (p23, p29, p60), and is discarded when the RX FIFO is full (p23)
 * - RX_ADDR_P0, RX_ADDR_P1 and TX_ADDR have their own 5-byte storage, and
 *   no register address steps within a transaction (p51)
 *
 * Uses hw_reg_model_t for the register file (0x00-0x1D),
 * plus custom SPI command parsing.
 */

#ifndef HW_MODEL_NRF24L01_H
#define HW_MODEL_NRF24L01_H

#include "hw_model.h"

/* --- nRF24L01+ register addresses --- */
#define NRF_REG_CONFIG      0x00
#define NRF_REG_EN_AA       0x01
#define NRF_REG_EN_RXADDR   0x02
#define NRF_REG_SETUP_AW    0x03
#define NRF_REG_SETUP_RETR  0x04
#define NRF_REG_RF_CH       0x05
#define NRF_REG_RF_SETUP    0x06
#define NRF_REG_STATUS      0x07
#define NRF_REG_OBSERVE_TX  0x08
#define NRF_REG_RPD         0x09
#define NRF_REG_RX_ADDR_P0  0x0A
#define NRF_REG_RX_ADDR_P1  0x0B
#define NRF_REG_RX_ADDR_P2  0x0C
#define NRF_REG_TX_ADDR     0x10
#define NRF_REG_RX_PW_P0    0x11
#define NRF_REG_FIFO_STATUS 0x17
#define NRF_REG_DYNPD       0x1C
#define NRF_REG_FEATURE     0x1D

/* --- SPI commands --- */
#define NRF_CMD_R_REGISTER   0x00
#define NRF_CMD_W_REGISTER   0x20
#define NRF_CMD_R_RX_PAYLOAD 0x61
#define NRF_CMD_W_TX_PAYLOAD 0xA0
#define NRF_CMD_FLUSH_TX     0xE1
#define NRF_CMD_FLUSH_RX     0xE2
#define NRF_CMD_NOP          0xFF

/* --- CONFIG, RF_SETUP and STATUS bits --- */
#define NRF_CONFIG_PWR_UP    0x02
#define NRF_CONFIG_PRIM_RX   0x01
#define NRF_RF_DR_LOW        0x20
#define NRF_STATUS_RX_DR     0x40
#define NRF_STATUS_TX_DS     0x20
#define NRF_STATUS_MAX_RT    0x10
#define NRF_STATUS_CLEARABLE 0x70
#define NRF_STATUS_RX_P_NO   0x0E  /* 111 = RX FIFO empty, 000 = pipe 0 */
#define NRF_STATUS_TX_FULL   0x01

#define NRF_FIFO_DEPTH       3

/* --- timing, Figure 4 on p22 and Table 16 on p24 --- */
#define NRF_T_POR_US         100000U
#define NRF_T_PD2STBY_US     1500U   /* crystal with Ls < 30 mH */
#define NRF_T_STBY2A_US      130U
#define NRF_T_HCE_US         10U

typedef struct {
    hw_reg_model_t hw;
    /* FIFOs, head at index 0 */
    uint8_t  tx_fifo[NRF_FIFO_DEPTH][32];
    uint8_t  tx_fifo_len[NRF_FIFO_DEPTH];
    uint8_t  tx_level;
    uint8_t  rx_fifo[NRF_FIFO_DEPTH][32];
    uint8_t  rx_fifo_len[NRF_FIFO_DEPTH];
    uint8_t  rx_fifo_pipe[NRF_FIFO_DEPTH];
    uint8_t  rx_count;
    /* Multi-byte registers */
    uint8_t  addr_p0[5];
    uint8_t  addr_p1[5];
    uint8_t  addr_tx[5];
    /* SPI transaction state */
    uint8_t  spi_cmd;
    int      spi_byte_idx;
    uint8_t  spi_reg;       /* target register for R/W */
    bool     spi_is_write;
    /* Time and CE state */
    uint32_t now_us;
    uint32_t standby_at_us; /* when Tpd2stby ends after PWR_UP */
    bool     ce_high;
    bool     ce_armed;      /* CE rose in standby */
    uint32_t ce_high_us;
    /* A transmission under way */
    bool     tx_active;
    uint32_t tx_left_us;
    /* Counters */
    int      tx_count;      /* transmissions started */
    int      delay_count;
    int      writes_in_rx;  /* W_REGISTER other than STATUS outside standby */
    int      ce_before_standby;
    /* TX behavior: set to get MAX_RT instead of an ACK */
    bool     force_max_rt;
} nrf24_model_t;

static uint8_t *nrf24_model_addr_reg(nrf24_model_t *m, uint8_t reg)
{
    if (reg == NRF_REG_RX_ADDR_P0) return m->addr_p0;
    if (reg == NRF_REG_RX_ADDR_P1) return m->addr_p1;
    if (reg == NRF_REG_TX_ADDR)    return m->addr_tx;
    return NULL;
}

static uint8_t nrf24_model_fifo_status(const nrf24_model_t *m)
{
    uint8_t v = 0;
    if (m->rx_count == 0) v |= 0x01;               /* RX_EMPTY */
    if (m->rx_count == NRF_FIFO_DEPTH) v |= 0x02;  /* RX_FULL */
    if (m->tx_level == 0) v |= 0x10;               /* TX_EMPTY */
    if (m->tx_level == NRF_FIFO_DEPTH) v |= 0x20;  /* TX_FULL */
    return v;
}

/* RX_P_NO and TX_FULL follow the FIFOs */
static void nrf24_model_update_status(nrf24_model_t *m)
{
    uint8_t status = m->hw.regs[NRF_REG_STATUS] & NRF_STATUS_CLEARABLE;
    if (m->rx_count == 0) {
        status |= NRF_STATUS_RX_P_NO;
    } else {
        status = (uint8_t)(status | (uint8_t)(m->rx_fifo_pipe[0] << 1));
    }
    if (m->tx_level == NRF_FIFO_DEPTH) status |= NRF_STATUS_TX_FULL;
    m->hw.regs[NRF_REG_STATUS] = status;
}

static bool nrf24_model_powered(const nrf24_model_t *m)
{
    return (m->hw.regs[NRF_REG_CONFIG] & NRF_CONFIG_PWR_UP) != 0;
}

static bool nrf24_model_prim_rx(const nrf24_model_t *m)
{
    return (m->hw.regs[NRF_REG_CONFIG] & NRF_CONFIG_PRIM_RX) != 0;
}

static bool nrf24_model_in_rx(const nrf24_model_t *m)
{
    return m->ce_high && nrf24_model_powered(m) && nrf24_model_prim_rx(m);
}

static void nrf24_model_pop_tx(nrf24_model_t *m)
{
    m->tx_level--;
    for (uint8_t i = 0; i < m->tx_level; i++) {
        memcpy(m->tx_fifo[i], m->tx_fifo[i + 1], sizeof(m->tx_fifo[i]));
        m->tx_fifo_len[i] = m->tx_fifo_len[i + 1];
    }
    nrf24_model_update_status(m);
}

static void nrf24_model_pop_rx(nrf24_model_t *m)
{
    m->rx_count--;
    for (uint8_t i = 0; i < m->rx_count; i++) {
        memcpy(m->rx_fifo[i], m->rx_fifo[i + 1], sizeof(m->rx_fifo[i]));
        m->rx_fifo_len[i] = m->rx_fifo_len[i + 1];
        m->rx_fifo_pipe[i] = m->rx_fifo_pipe[i + 1];
    }
    nrf24_model_update_status(m);
}

/* W_REGISTER, one data byte */
static void nrf24_model_write(nrf24_model_t *m, int idx, uint8_t val)
{
    uint8_t reg = m->spi_reg;
    uint8_t *addr = nrf24_model_addr_reg(m, reg);

    if (m->now_us < NRF_T_POR_US) return;

    if (reg == NRF_REG_STATUS) {
        /* Writing 1 clears a flag bit, RX_P_NO and TX_FULL are read-only */
        if (idx == 0) {
            m->hw.regs[NRF_REG_STATUS] = (uint8_t)(m->hw.regs[NRF_REG_STATUS] &
                                                   ~(val & NRF_STATUS_CLEARABLE));
        }
        return;
    }
    if (nrf24_model_in_rx(m) || m->tx_active) {
        m->writes_in_rx++;
        return;
    }
    if (addr) {
        if (idx < 5) addr[idx] = val;
        return;
    }
    if (idx == 0) {
        bool was_up = nrf24_model_powered(m);
        hw_model__write_reg(&m->hw, reg, val);
        if (!was_up && nrf24_model_powered(m)) {
            m->standby_at_us = m->now_us + NRF_T_PD2STBY_US;
        }
    }
}

/* --- SPI HAL callbacks --- */

__attribute__((unused))
static void nrf24_model_cs_low(void *ctx)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    m->spi_byte_idx = 0;
    m->spi_cmd = 0;
    m->spi_reg = 0;
    m->spi_is_write = false;
}

__attribute__((unused))
static void nrf24_model_cs_high(void *ctx)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    int data_bytes = m->spi_byte_idx - 1;

    /* A payload leaves the RX FIFO once it has been read */
    if (m->spi_cmd == NRF_CMD_R_RX_PAYLOAD && data_bytes > 0 && m->rx_count > 0) {
        nrf24_model_pop_rx(m);
    }
    /* An uploaded payload takes a level when CSN rises */
    if (m->spi_cmd == NRF_CMD_W_TX_PAYLOAD && data_bytes > 0 &&
        m->tx_level < NRF_FIFO_DEPTH) {
        m->tx_fifo_len[m->tx_level] = (uint8_t)(data_bytes > 32 ? 32 : data_bytes);
        m->tx_level++;
        nrf24_model_update_status(m);
    }
    m->spi_cmd = 0;
}

__attribute__((unused))
static uint8_t nrf24_model_xfer(void *ctx, uint8_t tx)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;

    if (m->spi_byte_idx == 0) {
        /* First byte: command, STATUS comes back */
        uint8_t status = m->hw.regs[NRF_REG_STATUS];
        m->spi_cmd = tx;
        m->spi_byte_idx = 1;

        if (tx == NRF_CMD_FLUSH_TX) {
            m->tx_level = 0;
            nrf24_model_update_status(m);
        } else if (tx == NRF_CMD_FLUSH_RX) {
            m->rx_count = 0;
            nrf24_model_update_status(m);
        } else if ((tx & 0xE0) == NRF_CMD_W_REGISTER) {
            m->spi_reg = tx & 0x1F;
            m->spi_is_write = true;
        } else if ((tx & 0xE0) == NRF_CMD_R_REGISTER) {
            m->spi_reg = tx & 0x1F;
        }
        return status;
    }

    /* Subsequent bytes */
    int idx = m->spi_byte_idx - 1;
    m->spi_byte_idx++;

    if (m->spi_cmd == NRF_CMD_W_TX_PAYLOAD) {
        if (idx < 32 && m->tx_level < NRF_FIFO_DEPTH) {
            m->tx_fifo[m->tx_level][idx] = tx;
        }
        return 0xFF;
    }

    if (m->spi_cmd == NRF_CMD_R_RX_PAYLOAD) {
        return (m->rx_count > 0 && idx < (int)m->rx_fifo_len[0])
                   ? m->rx_fifo[0][idx] : 0xFF;
    }

    if (m->spi_is_write) {
        nrf24_model_write(m, idx, tx);
        return 0xFF;
    }

    if ((m->spi_cmd & 0xE0) == NRF_CMD_R_REGISTER) {
        uint8_t *addr = nrf24_model_addr_reg(m, m->spi_reg);
        if (addr) return (idx < 5) ? addr[idx] : 0xFF;
        if (idx != 0) return 0xFF;
        if (m->spi_reg == NRF_REG_FIFO_STATUS) return nrf24_model_fifo_status(m);
        return m->hw.regs[m->spi_reg];
    }
    return 0xFF;
}

__attribute__((unused))
static void nrf24_model_ce_low(void *ctx)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    m->ce_high = false;
    m->ce_armed = false;
    m->ce_high_us = 0;
}

__attribute__((unused))
static void nrf24_model_ce_high(void *ctx)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    if (m->ce_high) return;

    bool standby = nrf24_model_powered(m) && m->now_us >= m->standby_at_us;
    if (nrf24_model_powered(m) && !standby) m->ce_before_standby++;
    m->ce_high = true;
    m->ce_armed = standby;
    m->ce_high_us = 0;
}

static bool nrf24_model_tx_can_start(const nrf24_model_t *m)
{
    return !m->tx_active && m->ce_armed && m->ce_high_us >= NRF_T_HCE_US &&
           nrf24_model_powered(m) && !nrf24_model_prim_rx(m) &&
           m->tx_level > 0 &&
           (m->hw.regs[NRF_REG_STATUS] & NRF_STATUS_MAX_RT) == 0;
}

static void nrf24_model_tx_finish(nrf24_model_t *m)
{
    bool slow = (m->hw.regs[NRF_REG_RF_SETUP] & NRF_RF_DR_LOW) != 0 &&
                (m->hw.regs[NRF_REG_EN_AA] & 0x01) != 0 &&
                (m->hw.regs[NRF_REG_SETUP_RETR] & 0xF0) == 0;

    m->tx_active = false;
    if (m->force_max_rt || slow) {
        /* No ACK: the payload stays in the TX FIFO */
        m->hw.regs[NRF_REG_STATUS] |= NRF_STATUS_MAX_RT;
    } else {
        m->hw.regs[NRF_REG_STATUS] |= NRF_STATUS_TX_DS;
        nrf24_model_pop_tx(m);
    }
}

__attribute__((unused))
static void nrf24_model_delay_us(void *ctx, uint32_t us)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    m->delay_count++;
    m->now_us += us;
    if (m->ce_high) m->ce_high_us += us;

    if (m->tx_active) {
        if (us >= m->tx_left_us) {
            nrf24_model_tx_finish(m);
        } else {
            m->tx_left_us -= us;
        }
    } else if (nrf24_model_tx_can_start(m)) {
        m->tx_active = true;
        m->tx_left_us = NRF_T_STBY2A_US;
        m->tx_count++;
    }
}

static void nrf24_model_init(nrf24_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR defaults (from the register map, p57 to p62) */
    hw_model_set_reset_value(&m->hw, NRF_REG_CONFIG, 0x08);      /* EN_CRC */
    hw_model_set_reset_value(&m->hw, NRF_REG_EN_AA, 0x3F);       /* AA on all pipes */
    hw_model_set_reset_value(&m->hw, NRF_REG_EN_RXADDR, 0x03);   /* pipes 0-1 enabled */
    hw_model_set_reset_value(&m->hw, NRF_REG_SETUP_AW, 0x03);    /* 5-byte address */
    hw_model_set_reset_value(&m->hw, NRF_REG_SETUP_RETR, 0x03);  /* 250 us, 3 retries */
    hw_model_set_reset_value(&m->hw, NRF_REG_RF_CH, 0x02);       /* channel 2 */
    hw_model_set_reset_value(&m->hw, NRF_REG_RF_SETUP, 0x0E);    /* 2Mbps, 0dBm */
    hw_model_set_reset_value(&m->hw, NRF_REG_STATUS, 0x0E);      /* reset value */
    for (uint8_t i = 0; i < 4; i++) {
        hw_model_set_reset_value(&m->hw, (uint8_t)(NRF_REG_RX_ADDR_P2 + i),
                                 (uint8_t)(0xC3 + i));
    }
    memset(m->addr_p0, 0xE7, sizeof(m->addr_p0));
    memset(m->addr_p1, 0xC2, sizeof(m->addr_p1));
    memset(m->addr_tx, 0xE7, sizeof(m->addr_tx));

    /* Bits the register map marks reserved or read-only */
    hw_model_set_writable_mask(&m->hw, NRF_REG_CONFIG, 0x7F);
    hw_model_set_writable_mask(&m->hw, NRF_REG_EN_AA, 0x3F);
    hw_model_set_writable_mask(&m->hw, NRF_REG_EN_RXADDR, 0x3F);
    hw_model_set_writable_mask(&m->hw, NRF_REG_SETUP_AW, 0x03);
    hw_model_set_writable_mask(&m->hw, NRF_REG_RF_CH, 0x7F);
    hw_model_set_writable_mask(&m->hw, NRF_REG_RF_SETUP, 0xBE);
    for (uint8_t i = 0; i < 6; i++) {
        hw_model_set_writable_mask(&m->hw, (uint8_t)(NRF_REG_RX_PW_P0 + i), 0x3F);
    }
    hw_model_set_readonly(&m->hw, NRF_REG_OBSERVE_TX);
    hw_model_set_readonly(&m->hw, NRF_REG_RPD);
    hw_model_set_readonly(&m->hw, NRF_REG_FIFO_STATUS);
    hw_model_set_writable_mask(&m->hw, NRF_REG_DYNPD, 0x3F);
    hw_model_set_writable_mask(&m->hw, NRF_REG_FEATURE, 0x07);

    /* STATUS register: writing 1 clears (handled in nrf24_model_write) */

    hw_model_reset(&m->hw);
}

/* Helper: a packet arrives on a pipe */
static void nrf24_model_inject_rx_pipe(nrf24_model_t *m, uint8_t pipe,
                                       const uint8_t *data, uint8_t len)
{
    if (!nrf24_model_in_rx(m) || pipe > 5) return;
    if ((m->hw.regs[NRF_REG_EN_RXADDR] & (1U << pipe)) == 0) return;
    if (len == 0 || len != m->hw.regs[NRF_REG_RX_PW_P0 + pipe]) return;
    if (m->rx_count == NRF_FIFO_DEPTH) return;  /* the chip discards it */

    memcpy(m->rx_fifo[m->rx_count], data, len);
    m->rx_fifo_len[m->rx_count] = len;
    m->rx_fifo_pipe[m->rx_count] = pipe;
    m->rx_count++;
    m->hw.regs[NRF_REG_STATUS] |= NRF_STATUS_RX_DR;
    nrf24_model_update_status(m);
}

/* Helper: a packet arrives on pipe 0 */
static void nrf24_model_inject_rx(nrf24_model_t *m, const uint8_t *data,
                                  uint8_t len)
{
    nrf24_model_inject_rx_pipe(m, 0, data, len);
}

#endif /* HW_MODEL_NRF24L01_H */
