// SPDX-License-Identifier: MIT
/*
 * cc1101 -- TI CC1101 Sub-GHz transceiver driver (SPI)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "cc1101.h"
#include "contract.h"
#include <string.h>

/* ---------- CC1101 registers ---------- */

#define REG_IOCFG2      0x00
#define REG_FIFOTHR     0x03
#define REG_PKTCTRL1    0x07
#define REG_PKTCTRL0    0x08
#define REG_ADDR        0x09
#define REG_CHANNR      0x0A
#define REG_FREQ2       0x0D
#define REG_FREQ1       0x0E
#define REG_FREQ0       0x0F
#define REG_MDMCFG4     0x10
#define REG_MDMCFG3     0x11
#define REG_MDMCFG2     0x12
#define REG_MDMCFG1     0x13
#define REG_MDMCFG0     0x14
#define REG_PATABLE     0x3E
#define REG_RXFIFO      0x3F
#define REG_TXFIFO      0x3F

/* Status registers (read with burst bit) */
#define REG_PARTNUM     0x30
#define REG_VERSION     0x31
#define REG_RSSI        0x34
#define REG_MARCSTATE   0x35
#define REG_RXBYTES     0x3B
#define REG_TXBYTES     0x3A

/* Command strobes */
#define STROBE_SRES     0x30
#define STROBE_SRX      0x34
#define STROBE_STX      0x35
#define STROBE_SIDLE    0x36
#define STROBE_SFRX     0x3A
#define STROBE_SFTX     0x3B

/* SPI header bits */
#define SPI_READ        0x80
#define SPI_BURST       0x40

/* Expected chip identity */
#define CC1101_PARTNUM  0x00
#define CC1101_VER      0x14

/* FIFO size */
#define FIFO_SIZE       64
#define MAX_PACKET_LEN  61   /* 64 - 1 (len byte) - 2 (status bytes) */

/* Oscillator frequency */
#define FXOSC           26000000UL

/* Polling limits */
#define POLL_MAX        200

/* MARCSTATE values */
#define MARCSTATE_IDLE  0x01
#define MARCSTATE_RX    0x0D
#define MARCSTATE_TX    0x13

/* ---------- internal helpers ---------- */

static uint8_t reg_read(const cc1101_spi_t *spi, uint8_t addr)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, (uint8_t)(addr | SPI_READ | SPI_BURST));
    uint8_t val = spi->xfer(spi->ctx, 0x00);
    spi->cs_high(spi->ctx);
    return val;
}

static void reg_write(const cc1101_spi_t *spi, uint8_t addr, uint8_t val)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, addr);
    spi->xfer(spi->ctx, val);
    spi->cs_high(spi->ctx);
}

static void strobe(const cc1101_spi_t *spi, uint8_t cmd)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, cmd);
    spi->cs_high(spi->ctx);
}

static uint8_t fifo_read_single(const cc1101_spi_t *spi)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, REG_RXFIFO | SPI_READ);
    uint8_t val = spi->xfer(spi->ctx, 0x00);
    spi->cs_high(spi->ctx);
    return val;
}

static void fifo_read_burst(const cc1101_spi_t *spi,
                            uint8_t *data, uint8_t len)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, REG_RXFIFO | SPI_READ | SPI_BURST);
    for (uint8_t i = 0; i < len; i++) {
        data[i] = spi->xfer(spi->ctx, 0x00);
    }
    spi->cs_high(spi->ctx);
}

/* ---------- public API ---------- */

cc1101_err_t cc1101_init(cc1101_t *dev, const cc1101_spi_t *spi,
                         const cc1101_cfg_t *cfg)
{
    if (!dev || !spi || !cfg) return CC1101_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer || !spi->delay_ms) {
        return CC1101_ERR_SPI;
    }

    dev->spi = spi;
    dev->cfg = *cfg;

    /* SRES -- chip reset */
    strobe(spi, STROBE_SRES);
    spi->delay_ms(spi->ctx, 5);

    /* Verify part number and version */
    uint8_t partnum = reg_read(spi, REG_PARTNUM);
    uint8_t version = reg_read(spi, REG_VERSION);
    if (partnum != CC1101_PARTNUM || version != CC1101_VER) {
        return CC1101_ERR_VERSION;
    }

    /* Set variable packet length mode, CRC autoflush, append status */
    reg_write(spi, REG_PKTCTRL1, 0x0C); /* CRC_AUTOFLUSH + APPEND_STATUS */
    reg_write(spi, REG_PKTCTRL0, 0x05); /* variable length, CRC enabled */

    /* Set data rate exponent from config */
    reg_write(spi, REG_MDMCFG4, (uint8_t)(0xC0 | (cfg->data_rate & 0x0F)));
    reg_write(spi, REG_MDMCFG3, 0x83); /* data rate mantissa default */

    /* Set modulation */
    uint8_t mdmcfg2 = (uint8_t)(0x02 | ((cfg->modulation & 0x07) << 4));
    reg_write(spi, REG_MDMCFG2, mdmcfg2);

    /* Set frequency */
    cc1101_err_t err = cc1101_set_frequency(dev, cfg->frequency_hz);
    if (err != CC1101_OK) return err;

    /* Go to IDLE */
    strobe(spi, STROBE_SIDLE);

    ENSURE(dev->spi != NULL);
    return CC1101_OK;
}

cc1101_err_t cc1101_set_frequency(cc1101_t *dev, uint32_t freq_hz)
{
    if (!dev || !dev->spi) return CC1101_ERR_PARAM;
    if (freq_hz < 300000000UL || freq_hz > 928000000UL) {
        return CC1101_ERR_PARAM;
    }

    /* FREQ = freq_hz * 2^16 / FXOSC */
    uint32_t freq_word = (uint32_t)(((uint64_t)freq_hz << 16) / FXOSC);

    const cc1101_spi_t *spi = dev->spi;
    reg_write(spi, REG_FREQ2, (uint8_t)(freq_word >> 16));
    reg_write(spi, REG_FREQ1, (uint8_t)(freq_word >> 8));
    reg_write(spi, REG_FREQ0, (uint8_t)(freq_word >> 0));

    dev->cfg.frequency_hz = freq_hz;
    return CC1101_OK;
}

cc1101_err_t cc1101_set_power(cc1101_t *dev, uint8_t pa_value)
{
    if (!dev || !dev->spi) return CC1101_ERR_PARAM;
    reg_write(dev->spi, REG_PATABLE, pa_value);
    return CC1101_OK;
}

cc1101_err_t cc1101_transmit(cc1101_t *dev, const uint8_t *data,
                             uint8_t len)
{
    if (!dev || !dev->spi) return CC1101_ERR_PARAM;
    if (!data || len == 0 || len > MAX_PACKET_LEN) {
        return CC1101_ERR_PARAM;
    }

    const cc1101_spi_t *spi = dev->spi;

    /* Flush TX FIFO */
    strobe(spi, STROBE_SIDLE);
    strobe(spi, STROBE_SFTX);

    /* Write length byte + payload */
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, REG_TXFIFO | SPI_BURST);
    spi->xfer(spi->ctx, len);
    for (uint8_t i = 0; i < len; i++) {
        spi->xfer(spi->ctx, data[i]);
    }
    spi->cs_high(spi->ctx);

    /* Start TX */
    strobe(spi, STROBE_STX);

    /* Poll for return to IDLE (TX complete) */
    for (int i = 0; i < POLL_MAX; i++) {
        spi->delay_ms(spi->ctx, 5);
        uint8_t state = reg_read(spi, REG_MARCSTATE) & 0x1F;
        if (state == MARCSTATE_IDLE) {
            return CC1101_OK;
        }
    }

    strobe(spi, STROBE_SIDLE);
    return CC1101_ERR_TIMEOUT;
}

cc1101_err_t cc1101_receive(cc1101_t *dev, uint8_t *buf, uint8_t capacity,
                            uint8_t *out_len)
{
    if (!dev || !dev->spi) return CC1101_ERR_PARAM;
    if (!buf || !out_len) return CC1101_ERR_PARAM;

    const cc1101_spi_t *spi = dev->spi;

    /* Flush RX FIFO and enter RX */
    strobe(spi, STROBE_SIDLE);
    strobe(spi, STROBE_SFRX);
    strobe(spi, STROBE_SRX);

    /* Poll for data in FIFO */
    for (int i = 0; i < POLL_MAX; i++) {
        spi->delay_ms(spi->ctx, 5);
        uint8_t rxbytes = reg_read(spi, REG_RXBYTES) & 0x7F;
        if (rxbytes > 0) {
            /* Read length byte (single read, not burst) */
            uint8_t pkt_len = fifo_read_single(spi);
            if (pkt_len > capacity) {
                pkt_len = capacity;
            }

            /* Read payload */
            fifo_read_burst(spi, buf, pkt_len);
            *out_len = pkt_len;

            /* Flush remaining */
            strobe(spi, STROBE_SIDLE);
            strobe(spi, STROBE_SFRX);
            return CC1101_OK;
        }
    }

    strobe(spi, STROBE_SIDLE);
    return CC1101_ERR_TIMEOUT;
}

cc1101_err_t cc1101_get_rssi(cc1101_t *dev, int16_t *rssi)
{
    if (!dev || !dev->spi || !rssi) return CC1101_ERR_PARAM;

    uint8_t raw = reg_read(dev->spi, REG_RSSI);

    /* CC1101 RSSI formula: if raw >= 128: RSSI = (raw-256)/2 - 74
     *                      else:          RSSI = raw/2 - 74         */
    int16_t rssi_val;
    if (raw >= 128) {
        rssi_val = (int16_t)((int16_t)(raw - 256) / 2 - 74);
    } else {
        rssi_val = (int16_t)((int16_t)raw / 2 - 74);
    }

    *rssi = rssi_val;
    return CC1101_OK;
}

cc1101_err_t cc1101_idle(cc1101_t *dev)
{
    if (!dev || !dev->spi) return CC1101_ERR_PARAM;
    strobe(dev->spi, STROBE_SIDLE);
    return CC1101_OK;
}