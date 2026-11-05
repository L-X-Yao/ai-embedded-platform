// SPDX-License-Identifier: MIT
/*
 * nrf24l01 -- SPI driver for Nordic nRF24L01+ 2.4 GHz radio transceiver
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "nrf24l01.h"
#include "contract.h"

/* ---------- nRF24L01+ SPI commands ---------- */

#define CMD_R_REGISTER      0x00    /* | reg addr (5 bits) */
#define CMD_W_REGISTER      0x20    /* | reg addr (5 bits) */
#define CMD_R_RX_PAYLOAD    0x61
#define CMD_W_TX_PAYLOAD    0xA0
#define CMD_FLUSH_TX        0xE1
#define CMD_FLUSH_RX        0xE2
#define CMD_NOP             0xFF

/* ---------- register addresses ---------- */

#define REG_CONFIG          0x00
#define REG_EN_AA           0x01
#define REG_EN_RXADDR       0x02
#define REG_SETUP_AW        0x03
#define REG_RF_CH           0x05
#define REG_RF_SETUP        0x06
#define REG_STATUS          0x07
#define REG_RX_ADDR_P0      0x0A
#define REG_RX_ADDR_P1      0x0B
#define REG_RX_ADDR_P2      0x0C
#define REG_RX_ADDR_P3      0x0D
#define REG_RX_ADDR_P4      0x0E
#define REG_RX_ADDR_P5      0x0F
#define REG_TX_ADDR         0x10
#define REG_RX_PW_P0        0x11
#define REG_FIFO_STATUS     0x17

/* ---------- register bits ---------- */

#define CONFIG_EN_CRC       0x08
#define CONFIG_CRCO         0x04
#define CONFIG_PWR_UP       0x02
#define CONFIG_PRIM_RX      0x01

#define STATUS_RX_DR        0x40
#define STATUS_TX_DS        0x20
#define STATUS_MAX_RT       0x10

#define FIFO_RX_EMPTY       0x01

/* RF_SETUP bits for data rate:
 *   250kbps: RF_DR_LOW=1, RF_DR_HIGH=0
 *   1Mbps:   RF_DR_LOW=0, RF_DR_HIGH=0
 *   2Mbps:   RF_DR_LOW=0, RF_DR_HIGH=1
 */
#define RF_SETUP_RF_DR_LOW  0x20
#define RF_SETUP_RF_DR_HIGH 0x08
#define RF_SETUP_RF_PWR_MASK 0x06

/* ---------- timing ---------- */

#ifndef NRF24_TX_TIMEOUT_US
#define NRF24_TX_TIMEOUT_US 10000U
#endif

#define CE_PULSE_US         10U
#define POWER_ON_DELAY_US   1500U

/* ---------- internal helpers ---------- */

static uint8_t read_reg(const nrf24_spi_t *s, uint8_t reg)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, (uint8_t)(CMD_R_REGISTER | (reg & 0x1F)));
    uint8_t val = s->xfer(s->ctx, CMD_NOP);
    s->cs_high(s->ctx);
    return val;
}

static void write_reg(const nrf24_spi_t *s, uint8_t reg, uint8_t val)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, (uint8_t)(CMD_W_REGISTER | (reg & 0x1F)));
    s->xfer(s->ctx, val);
    s->cs_high(s->ctx);
}

static void write_reg_multi(const nrf24_spi_t *s, uint8_t reg,
                            const uint8_t *data, uint8_t len)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, (uint8_t)(CMD_W_REGISTER | (reg & 0x1F)));
    for (uint8_t i = 0; i < len; i++) {
        s->xfer(s->ctx, data[i]);
    }
    s->cs_high(s->ctx);
}

static void spi_cmd(const nrf24_spi_t *s, uint8_t cmd)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, cmd);
    s->cs_high(s->ctx);
}

static uint8_t get_status(const nrf24_spi_t *s)
{
    s->cs_low(s->ctx);
    uint8_t st = s->xfer(s->ctx, CMD_NOP);
    s->cs_high(s->ctx);
    return st;
}

/* ---------- API ---------- */

nrf24_err_t nrf24_init(nrf24_t *dev, const nrf24_spi_t *spi,
                       uint8_t channel, uint8_t payload_size)
{
    if (!dev || !spi || !spi->cs_low || !spi->cs_high || !spi->xfer
        || !spi->ce_low || !spi->ce_high || !spi->delay_us) {
        return NRF24_ERR_SPI;
    }
    if (channel > 125 || payload_size < 1 || payload_size > 32) {
        return NRF24_ERR_PARAM;
    }

    dev->spi          = spi;
    dev->channel      = channel;
    dev->payload_size = payload_size;
    dev->mode         = NRF24_MODE_TX;

    /* CE low during configuration */
    spi->ce_low(spi->ctx);

    /* Power-on reset delay */
    spi->delay_us(spi->ctx, POWER_ON_DELAY_US);

    /* CONFIG: PWR_UP + EN_CRC + CRC 2-byte, TX mode */
    write_reg(spi, REG_CONFIG,
              CONFIG_PWR_UP | CONFIG_EN_CRC | CONFIG_CRCO);

    /* Startup delay after PWR_UP */
    spi->delay_us(spi->ctx, POWER_ON_DELAY_US);

    /* Set channel */
    write_reg(spi, REG_RF_CH, channel);

    /* Enable auto-ack on pipe 0 */
    write_reg(spi, REG_EN_AA, 0x01);

    /* Enable RX pipe 0 */
    write_reg(spi, REG_EN_RXADDR, 0x01);

    /* Set payload size for pipe 0 */
    write_reg(spi, REG_RX_PW_P0, payload_size);

    /* Clear status flags */
    write_reg(spi, REG_STATUS,
              STATUS_RX_DR | STATUS_TX_DS | STATUS_MAX_RT);

    /* Flush both FIFOs */
    spi_cmd(spi, CMD_FLUSH_TX);
    spi_cmd(spi, CMD_FLUSH_RX);

    return NRF24_OK;
}

nrf24_err_t nrf24_set_tx_addr(nrf24_t *dev, const uint8_t *addr,
                              uint8_t addr_len)
{
    if (!dev || !dev->spi || !addr) return NRF24_ERR_SPI;
    if (addr_len < 3 || addr_len > 5) return NRF24_ERR_PARAM;

    const nrf24_spi_t *s = dev->spi;

    /* Set address width: register value = addr_len - 2 */
    write_reg(s, REG_SETUP_AW, (uint8_t)(addr_len - 2));

    /* Write TX address */
    write_reg_multi(s, REG_TX_ADDR, addr, addr_len);

    /* Set RX_ADDR_P0 to same address for auto-ack */
    write_reg_multi(s, REG_RX_ADDR_P0, addr, addr_len);

    return NRF24_OK;
}

nrf24_err_t nrf24_set_rx_addr(nrf24_t *dev, uint8_t pipe,
                              const uint8_t *addr, uint8_t addr_len)
{
    if (!dev || !dev->spi || !addr) return NRF24_ERR_SPI;
    if (pipe > 5) return NRF24_ERR_PARAM;
    if (addr_len < 1 || addr_len > 5) return NRF24_ERR_PARAM;
    /* Pipes 2-5 use single-byte addresses */
    if (pipe >= 2 && addr_len != 1) return NRF24_ERR_PARAM;
    /* Pipes 0-1 need 3-5 byte addresses */
    if (pipe <= 1 && addr_len < 3) return NRF24_ERR_PARAM;

    const nrf24_spi_t *s = dev->spi;
    uint8_t reg = (uint8_t)(REG_RX_ADDR_P0 + pipe);

    if (pipe <= 1) {
        write_reg_multi(s, reg, addr, addr_len);
    } else {
        write_reg(s, reg, addr[0]);
    }

    /* A width of 0 means the pipe is not used */
    write_reg(s, (uint8_t)(REG_RX_PW_P0 + pipe), dev->payload_size);

    /* Enable RX on this pipe */
    uint8_t en = read_reg(s, REG_EN_RXADDR);
    en = (uint8_t)(en | (uint8_t)(1U << pipe));
    write_reg(s, REG_EN_RXADDR, en);

    return NRF24_OK;
}

nrf24_err_t nrf24_set_mode(nrf24_t *dev, nrf24_mode_t mode)
{
    if (!dev || !dev->spi) return NRF24_ERR_SPI;

    const nrf24_spi_t *s = dev->spi;
    uint8_t cfg = read_reg(s, REG_CONFIG);

    if (mode == NRF24_MODE_RX) {
        cfg = (uint8_t)(cfg | CONFIG_PRIM_RX);
    } else {
        cfg = (uint8_t)(cfg & (uint8_t)~CONFIG_PRIM_RX);
    }
    write_reg(s, REG_CONFIG, cfg);
    dev->mode = mode;

    if (mode == NRF24_MODE_RX) {
        /* CE high to start listening */
        s->ce_high(s->ctx);
    } else {
        s->ce_low(s->ctx);
    }

    return NRF24_OK;
}

nrf24_err_t nrf24_set_data_rate(nrf24_t *dev, nrf24_data_rate_t rate)
{
    if (!dev || !dev->spi) return NRF24_ERR_SPI;
    if (rate > NRF24_RATE_2MBPS) return NRF24_ERR_PARAM;

    const nrf24_spi_t *s = dev->spi;
    uint8_t setup = read_reg(s, REG_RF_SETUP);

    /* Clear data rate bits */
    setup = (uint8_t)(setup & (uint8_t)~(RF_SETUP_RF_DR_LOW | RF_SETUP_RF_DR_HIGH));

    switch (rate) {
    case NRF24_RATE_250KBPS:
        setup = (uint8_t)(setup | RF_SETUP_RF_DR_LOW);
        break;
    case NRF24_RATE_1MBPS:
        /* Both bits 0 = 1Mbps (default) */
        break;
    case NRF24_RATE_2MBPS:
        setup = (uint8_t)(setup | RF_SETUP_RF_DR_HIGH);
        break;
    default:
        break;
    }

    write_reg(s, REG_RF_SETUP, setup);
    return NRF24_OK;
}

nrf24_err_t nrf24_set_power(nrf24_t *dev, nrf24_power_t power)
{
    if (!dev || !dev->spi) return NRF24_ERR_SPI;
    if (power > NRF24_POWER_0DBM) return NRF24_ERR_PARAM;

    const nrf24_spi_t *s = dev->spi;
    uint8_t setup = read_reg(s, REG_RF_SETUP);

    /* Clear power bits [2:1], then set */
    setup = (uint8_t)(setup & (uint8_t)~RF_SETUP_RF_PWR_MASK);
    setup = (uint8_t)(setup | (uint8_t)((uint8_t)power << 1));

    write_reg(s, REG_RF_SETUP, setup);
    return NRF24_OK;
}

nrf24_err_t nrf24_send(nrf24_t *dev, const uint8_t *data, uint8_t len)
{
    if (!dev || !dev->spi || !data) return NRF24_ERR_SPI;
    if (len < 1 || len > 32) return NRF24_ERR_PARAM;

    const nrf24_spi_t *s = dev->spi;

    /* Write TX payload */
    s->cs_low(s->ctx);
    s->xfer(s->ctx, CMD_W_TX_PAYLOAD);
    for (uint8_t i = 0; i < len; i++) {
        s->xfer(s->ctx, data[i]);
    }
    s->cs_high(s->ctx);

    /* CE pulse to trigger transmission */
    s->ce_high(s->ctx);
    s->delay_us(s->ctx, CE_PULSE_US);
    s->ce_low(s->ctx);

    /* Poll STATUS for TX_DS or MAX_RT */
    uint32_t elapsed = 0;
    uint8_t status;
    while (elapsed < NRF24_TX_TIMEOUT_US) {
        status = get_status(s);
        if (status & STATUS_TX_DS) {
            /* Clear TX_DS flag */
            write_reg(s, REG_STATUS, STATUS_TX_DS);
            return NRF24_OK;
        }
        if (status & STATUS_MAX_RT) {
            /* Clear MAX_RT flag, flush TX */
            write_reg(s, REG_STATUS, STATUS_MAX_RT);
            spi_cmd(s, CMD_FLUSH_TX);
            return NRF24_ERR_MAX_RT;
        }
        s->delay_us(s->ctx, 10);
        elapsed += 10;
    }

    return NRF24_ERR_TIMEOUT;
}

nrf24_err_t nrf24_receive(nrf24_t *dev, uint8_t *data, uint8_t max_len,
                          uint8_t *len)
{
    if (!dev || !dev->spi || !data || !len) return NRF24_ERR_SPI;

    const nrf24_spi_t *s = dev->spi;

    /* RX_DR marks an arrival, but up to three payloads can wait in the FIFO */
    if (read_reg(s, REG_FIFO_STATUS) & FIFO_RX_EMPTY) {
        *len = 0;
        return NRF24_OK;
    }

    /* Ensure caller buffer can hold the payload */
    uint8_t count = dev->payload_size;
    if (count > max_len) return NRF24_ERR_PARAM;

    /* Read RX payload */
    s->cs_low(s->ctx);
    s->xfer(s->ctx, CMD_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < count; i++) {
        data[i] = s->xfer(s->ctx, CMD_NOP);
    }
    s->cs_high(s->ctx);

    *len = count;

    /* Clear RX_DR flag */
    write_reg(s, REG_STATUS, STATUS_RX_DR);

    return NRF24_OK;
}

bool nrf24_data_ready(nrf24_t *dev)
{
    if (!dev || !dev->spi) return false;
    return (read_reg(dev->spi, REG_FIFO_STATUS) & FIFO_RX_EMPTY) == 0;
}

nrf24_err_t nrf24_flush_tx(nrf24_t *dev)
{
    if (!dev || !dev->spi) return NRF24_ERR_SPI;
    spi_cmd(dev->spi, CMD_FLUSH_TX);
    return NRF24_OK;
}

nrf24_err_t nrf24_flush_rx(nrf24_t *dev)
{
    if (!dev || !dev->spi) return NRF24_ERR_SPI;
    spi_cmd(dev->spi, CMD_FLUSH_RX);
    return NRF24_OK;
}

uint8_t nrf24_read_status(nrf24_t *dev)
{
    if (!dev || !dev->spi) return 0;
    return get_status(dev->spi);
}
