// SPDX-License-Identifier: MIT
/*
 * hw_model_nrf24l01.h -- nRF24L01+ hardware register model
 *
 * Models Nordic nRF24L01+ SPI register behavior:
 * - SPI command protocol: R_REGISTER (0x00|reg), W_REGISTER (0x20|reg)
 * - STATUS register returned on first byte of every transaction
 * - Config 0x00, EN_AA 0x01, EN_RXADDR 0x02, SETUP_AW 0x03
 * - RF_CH 0x05, RF_SETUP 0x06, STATUS 0x07
 * - TX/RX address registers (multi-byte, each in its own storage)
 * - RX FIFO of three payloads that pop on read, FIFO_STATUS computed from
 *   the FIFOs (TX FIFO stays single-entry)
 * - Register writes other than STATUS are counted while the chip is in RX
 * - Status flags: RX_DR (bit 6), TX_DS (bit 5), MAX_RT (bit 4)
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
#define NRF_REG_RX_ADDR_P0  0x0A
#define NRF_REG_RX_ADDR_P1  0x0B
#define NRF_REG_TX_ADDR     0x10
#define NRF_REG_RX_PW_P0    0x11
#define NRF_REG_FIFO_STATUS 0x17

/* --- SPI commands --- */
#define NRF_CMD_R_REGISTER   0x00
#define NRF_CMD_W_REGISTER   0x20
#define NRF_CMD_R_RX_PAYLOAD 0x61
#define NRF_CMD_W_TX_PAYLOAD 0xA0
#define NRF_CMD_FLUSH_TX     0xE1
#define NRF_CMD_FLUSH_RX     0xE2
#define NRF_CMD_NOP          0xFF

/* --- STATUS bits --- */
#define NRF_STATUS_RX_DR     0x40
#define NRF_STATUS_TX_DS     0x20
#define NRF_STATUS_MAX_RT    0x10
#define NRF_STATUS_CLEARABLE 0x70
#define NRF_STATUS_RX_P_NO   0x0E  /* 111 = RX FIFO empty, 000 = pipe 0 */

#define NRF_RX_FIFO_DEPTH    3

typedef struct {
    hw_reg_model_t hw;
    /* FIFO simulation */
    uint8_t  tx_fifo[32];
    uint8_t  tx_fifo_len;
    uint8_t  rx_fifo[NRF_RX_FIFO_DEPTH][32];
    uint8_t  rx_fifo_len[NRF_RX_FIFO_DEPTH];
    uint8_t  rx_count;
    bool     tx_fifo_has_data;
    /* Multi-byte registers */
    uint8_t  addr_p0[5];
    uint8_t  addr_p1[5];
    uint8_t  addr_tx[5];
    /* SPI transaction state */
    uint8_t  spi_cmd;
    int      spi_byte_idx;
    uint8_t  spi_reg;       /* target register for R/W */
    bool     spi_is_write;
    /* CE state */
    bool     ce_high;
    /* Counters */
    int      tx_count;      /* transmissions triggered */
    int      delay_count;
    int      writes_in_rx;  /* W_REGISTER other than STATUS with PRIM_RX and CE high */
    /* TX behavior: set to force TX_DS or MAX_RT */
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
    if (m->rx_count == 0) v |= 0x01;                  /* RX_EMPTY */
    if (m->rx_count == NRF_RX_FIFO_DEPTH) v |= 0x02;  /* RX_FULL */
    if (!m->tx_fifo_has_data) v |= 0x10;              /* TX_EMPTY */
    return v;
}

static void nrf24_model_update_rx_p_no(nrf24_model_t *m)
{
    uint8_t status = m->hw.regs[NRF_REG_STATUS] & (uint8_t)~NRF_STATUS_RX_P_NO;
    if (m->rx_count == 0) status |= NRF_STATUS_RX_P_NO;
    m->hw.regs[NRF_REG_STATUS] = status;
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

    /* A payload leaves the RX FIFO once it has been read */
    if (m->spi_cmd == NRF_CMD_R_RX_PAYLOAD && m->rx_count > 0) {
        m->rx_count--;
        for (uint8_t i = 0; i < m->rx_count; i++) {
            memcpy(m->rx_fifo[i], m->rx_fifo[i + 1], sizeof(m->rx_fifo[i]));
            m->rx_fifo_len[i] = m->rx_fifo_len[i + 1];
        }
        nrf24_model_update_rx_p_no(m);
    }
    m->spi_cmd = 0;
}

__attribute__((unused))
static uint8_t nrf24_model_xfer(void *ctx, uint8_t tx)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    uint8_t status = m->hw.regs[NRF_REG_STATUS];

    if (m->spi_byte_idx == 0) {
        /* First byte: command */
        m->spi_cmd = tx;
        m->spi_byte_idx = 1;

        /* Decode command */
        if (tx == NRF_CMD_NOP || tx == NRF_CMD_FLUSH_TX || tx == NRF_CMD_FLUSH_RX) {
            if (tx == NRF_CMD_FLUSH_TX) {
                m->tx_fifo_has_data = false;
                m->tx_fifo_len = 0;
            }
            if (tx == NRF_CMD_FLUSH_RX) {
                m->rx_count = 0;
                nrf24_model_update_rx_p_no(m);
            }
            return status;
        }
        if (tx == NRF_CMD_W_TX_PAYLOAD) {
            m->spi_byte_idx = 1;
            return status;
        }
        if (tx == NRF_CMD_R_RX_PAYLOAD) {
            m->spi_byte_idx = 1;
            return status;
        }
        /* R_REGISTER or W_REGISTER */
        if ((tx & 0xE0) == NRF_CMD_W_REGISTER) {
            m->spi_reg = tx & 0x1F;
            m->spi_is_write = true;
        } else {
            m->spi_reg = tx & 0x1F;
            m->spi_is_write = false;
        }
        return status;
    }

    /* Subsequent bytes */
    uint8_t result = 0xFF;

    if (m->spi_cmd == NRF_CMD_W_TX_PAYLOAD) {
        /* Write TX payload */
        int idx = m->spi_byte_idx - 1;
        if (idx < 32) {
            m->tx_fifo[idx] = tx;
            if ((uint8_t)(idx + 1) > m->tx_fifo_len)
                m->tx_fifo_len = (uint8_t)(idx + 1);
            m->tx_fifo_has_data = true;
        }
        m->spi_byte_idx++;
        return 0xFF;
    }

    if (m->spi_cmd == NRF_CMD_R_RX_PAYLOAD) {
        /* Read RX payload */
        int idx = m->spi_byte_idx - 1;
        result = (m->rx_count > 0 && idx < (int)m->rx_fifo_len[0])
                     ? m->rx_fifo[0][idx] : 0xFF;
        m->spi_byte_idx++;
        return result;
    }

    if (m->spi_is_write) {
        /* W_REGISTER: write to model register */
        uint8_t reg = (uint8_t)(m->spi_reg + (uint8_t)(m->spi_byte_idx - 1));
        uint8_t *addr = nrf24_model_addr_reg(m, m->spi_reg);
        bool in_rx = m->ce_high && (m->hw.regs[NRF_REG_CONFIG] & 0x01) != 0;
        if (m->spi_reg == NRF_REG_STATUS) {
            /* STATUS: writing 1 clears a flag bit, RX_P_NO is read-only */
            m->hw.regs[NRF_REG_STATUS] = (uint8_t)(m->hw.regs[NRF_REG_STATUS] &
                                                   ~(tx & NRF_STATUS_CLEARABLE));
        } else {
            if (in_rx) m->writes_in_rx++;
            if (addr) {
                if (m->spi_byte_idx <= 5) addr[m->spi_byte_idx - 1] = tx;
            } else {
                hw_model__write_reg(&m->hw, reg, tx);
            }
        }
        m->spi_byte_idx++;
        return 0xFF;
    }

    /* R_REGISTER: read from model register */
    {
        uint8_t reg = (uint8_t)(m->spi_reg + (uint8_t)(m->spi_byte_idx - 1));
        uint8_t *addr = nrf24_model_addr_reg(m, m->spi_reg);
        if (addr) {
            result = (m->spi_byte_idx <= 5) ? addr[m->spi_byte_idx - 1] : 0xFF;
        } else if (reg == NRF_REG_FIFO_STATUS) {
            result = nrf24_model_fifo_status(m);
        } else {
            result = m->hw.regs[reg];
        }
        m->spi_byte_idx++;
    }
    return result;
}

__attribute__((unused))
static void nrf24_model_ce_low(void *ctx)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    m->ce_high = false;
}

__attribute__((unused))
static void nrf24_model_ce_high(void *ctx)
{
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    m->ce_high = true;

    /* If in TX mode and FIFO has data, simulate transmission */
    uint8_t config = m->hw.regs[NRF_REG_CONFIG];
    bool prim_rx = (config & 0x01) != 0;

    if (!prim_rx && m->tx_fifo_has_data) {
        m->tx_count++;

        if (m->force_max_rt) {
            /* Simulate MAX_RT: the payload stays in the TX FIFO */
            m->hw.regs[NRF_REG_STATUS] |= NRF_STATUS_MAX_RT;
        } else {
            /* Simulate successful TX */
            m->tx_fifo_has_data = false;
            m->tx_fifo_len = 0;
            m->hw.regs[NRF_REG_STATUS] |= NRF_STATUS_TX_DS;
        }
    }
}

__attribute__((unused))
static void nrf24_model_delay_us(void *ctx, uint32_t us)
{
    (void)us;
    nrf24_model_t *m = (nrf24_model_t *)ctx;
    m->delay_count++;
}

static void nrf24_model_init(nrf24_model_t *m)
{
    memset(m, 0, sizeof(*m));
    hw_model_init(&m->hw);
    m->hw.user = m;

    /* POR defaults (from datasheet Table 22) */
    hw_model_set_reset_value(&m->hw, NRF_REG_CONFIG, 0x08);      /* EN_CRC */
    hw_model_set_reset_value(&m->hw, NRF_REG_EN_AA, 0x3F);       /* AA on all pipes */
    hw_model_set_reset_value(&m->hw, NRF_REG_EN_RXADDR, 0x03);   /* pipes 0-1 enabled */
    hw_model_set_reset_value(&m->hw, NRF_REG_SETUP_AW, 0x03);    /* 5-byte address */
    hw_model_set_reset_value(&m->hw, NRF_REG_SETUP_RETR, 0x03);  /* 250 us, 3 retries */
    hw_model_set_reset_value(&m->hw, NRF_REG_RF_CH, 0x02);       /* channel 2 */
    hw_model_set_reset_value(&m->hw, NRF_REG_RF_SETUP, 0x0E);    /* 2Mbps, 0dBm */
    hw_model_set_reset_value(&m->hw, NRF_REG_STATUS, 0x0E);      /* reset value */

    /* STATUS register: writing 1 clears (handled in xfer) */

    hw_model_reset(&m->hw);
}

/* Helper: inject RX data (simulates receiving a packet) */
static void nrf24_model_inject_rx(nrf24_model_t *m, const uint8_t *data,
                                  uint8_t len)
{
    if (m->rx_count == NRF_RX_FIFO_DEPTH) return;  /* the chip drops it */
    if (len > 32) len = 32;
    memcpy(m->rx_fifo[m->rx_count], data, len);
    m->rx_fifo_len[m->rx_count] = len;
    m->rx_count++;
    m->hw.regs[NRF_REG_STATUS] |= NRF_STATUS_RX_DR;
    nrf24_model_update_rx_p_no(m);
}

#endif /* HW_MODEL_NRF24L01_H */
