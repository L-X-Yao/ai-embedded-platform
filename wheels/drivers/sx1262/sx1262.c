// SPDX-License-Identifier: MIT
/*
 * sx1262 -- SPI LoRa/FSK transceiver driver for Semtech SX1262
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "sx1262.h"
#include "contract.h"
#include <string.h>

/* ---------- SX1262 command opcodes ---------- */

#define CMD_SET_SLEEP               0x84
#define CMD_SET_STANDBY             0x80
#define CMD_SET_TX                  0x83
#define CMD_SET_RX                  0x82
#define CMD_SET_PACKET_TYPE         0x8A
#define CMD_SET_RF_FREQUENCY        0x86
#define CMD_SET_TX_PARAMS           0x8E
#define CMD_SET_MODULATION_PARAMS   0x8B
#define CMD_SET_PACKET_PARAMS       0x8C
#define CMD_SET_BUFFER_BASE_ADDR    0x8F
#define CMD_WRITE_BUFFER            0x0E
#define CMD_READ_BUFFER             0x1E
#define CMD_GET_STATUS              0xC0
#define CMD_GET_RX_BUFFER_STATUS    0x13
#define CMD_GET_PACKET_STATUS       0x14
#define CMD_GET_IRQ_STATUS          0x12
#define CMD_CLEAR_IRQ_STATUS        0x02
#define CMD_WRITE_REGISTER          0x0D
#define CMD_READ_REGISTER           0x1D

/* NOP byte for SPI reads */
#define SPI_NOP                     0x00

/* Standby mode: RC oscillator */
#define STDBY_RC                    0x00

/* IRQ flags (16-bit) */
#define IRQ_TX_DONE                 0x0001
#define IRQ_RX_DONE                 0x0002
#define IRQ_TIMEOUT                 0x0200
#define IRQ_ALL                     0x03FF

/* Max polling iterations */
#define POLL_MAX                    200

/* Busy pin poll limit */
#define BUSY_TIMEOUT                100

/* ---------- internal helpers ---------- */

static sx1262_err_t wait_busy(const sx1262_spi_t *spi)
{
    for (int i = 0; i < BUSY_TIMEOUT; i++) {
        if (!spi->is_busy(spi->ctx)) {
            return SX1262_OK;
        }
        spi->delay_ms(spi->ctx, 1);
    }
    return SX1262_ERR_BUSY;
}

static sx1262_err_t send_command(const sx1262_spi_t *spi, uint8_t opcode,
                                 const uint8_t *params, uint8_t param_len)
{
    sx1262_err_t err = wait_busy(spi);
    if (err != SX1262_OK) return err;

    spi->cs_select(spi->ctx);
    spi->transfer(spi->ctx, opcode);
    for (uint8_t i = 0; i < param_len; i++) {
        spi->transfer(spi->ctx, params[i]);
    }
    spi->cs_deselect(spi->ctx);
    return SX1262_OK;
}

static sx1262_err_t read_command(const sx1262_spi_t *spi, uint8_t opcode,
                                 uint8_t *out, uint8_t out_len)
{
    sx1262_err_t err = wait_busy(spi);
    if (err != SX1262_OK) return err;

    spi->cs_select(spi->ctx);
    spi->transfer(spi->ctx, opcode);
    spi->transfer(spi->ctx, SPI_NOP); /* status byte */
    for (uint8_t i = 0; i < out_len; i++) {
        out[i] = spi->transfer(spi->ctx, SPI_NOP);
    }
    spi->cs_deselect(spi->ctx);
    return SX1262_OK;
}

static sx1262_err_t write_buffer(const sx1262_spi_t *spi, uint8_t offset,
                                 const uint8_t *data, uint8_t len)
{
    sx1262_err_t err = wait_busy(spi);
    if (err != SX1262_OK) return err;

    spi->cs_select(spi->ctx);
    spi->transfer(spi->ctx, CMD_WRITE_BUFFER);
    spi->transfer(spi->ctx, offset);
    for (uint8_t i = 0; i < len; i++) {
        spi->transfer(spi->ctx, data[i]);
    }
    spi->cs_deselect(spi->ctx);
    return SX1262_OK;
}

static sx1262_err_t read_buffer(const sx1262_spi_t *spi, uint8_t offset,
                                uint8_t *data, uint8_t len)
{
    sx1262_err_t err = wait_busy(spi);
    if (err != SX1262_OK) return err;

    spi->cs_select(spi->ctx);
    spi->transfer(spi->ctx, CMD_READ_BUFFER);
    spi->transfer(spi->ctx, offset);
    spi->transfer(spi->ctx, SPI_NOP); /* status */
    for (uint8_t i = 0; i < len; i++) {
        data[i] = spi->transfer(spi->ctx, SPI_NOP);
    }
    spi->cs_deselect(spi->ctx);
    return SX1262_OK;
}

static sx1262_err_t get_irq_status(const sx1262_spi_t *spi, uint16_t *irq)
{
    uint8_t buf[2];
    sx1262_err_t err = read_command(spi, CMD_GET_IRQ_STATUS, buf, 2);
    if (err != SX1262_OK) return err;
    *irq = (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
    return SX1262_OK;
}

static sx1262_err_t clear_irq(const sx1262_spi_t *spi, uint16_t mask)
{
    uint8_t params[2];
    params[0] = (uint8_t)(mask >> 8);
    params[1] = (uint8_t)(mask & 0xFF);
    return send_command(spi, CMD_CLEAR_IRQ_STATUS, params, 2);
}

/* ---------- public API ---------- */

sx1262_err_t sx1262_init(sx1262_t *dev, const sx1262_spi_t *spi,
                         const sx1262_cfg_t *cfg)
{
    if (!dev || !spi || !cfg) return SX1262_ERR_PARAM;
    if (!spi->cs_select || !spi->cs_deselect || !spi->transfer ||
        !spi->is_busy || !spi->delay_ms) {
        return SX1262_ERR_SPI;
    }

    dev->spi = spi;
    dev->cfg = *cfg;
    dev->packet_type = SX1262_PACKET_LORA;

    /* SetStandby(RC) */
    uint8_t stdby_param = STDBY_RC;
    sx1262_err_t err = send_command(spi, CMD_SET_STANDBY, &stdby_param, 1);
    if (err != SX1262_OK) return err;

    /* SetPacketType(LoRa) */
    uint8_t pkt_type = (uint8_t)SX1262_PACKET_LORA;
    err = send_command(spi, CMD_SET_PACKET_TYPE, &pkt_type, 1);
    if (err != SX1262_OK) return err;

    /* Set frequency */
    err = sx1262_set_frequency(dev, cfg->frequency_hz);
    if (err != SX1262_OK) return err;

    /* Configure LoRa modulation */
    err = sx1262_configure_lora(dev, cfg->spreading_factor,
                                cfg->bandwidth, cfg->coding_rate);
    if (err != SX1262_OK) return err;

    /* SetTxParams(power, rampTime=0x04 = 200us) */
    uint8_t tx_params[2];
    tx_params[0] = (uint8_t)cfg->tx_power_dbm;
    tx_params[1] = 0x04; /* 200us ramp time */
    err = send_command(spi, CMD_SET_TX_PARAMS, tx_params, 2);
    if (err != SX1262_OK) return err;

    /* SetBufferBaseAddress(TX=0x00, RX=0x80) */
    uint8_t buf_base[2] = {0x00, 0x80};
    err = send_command(spi, CMD_SET_BUFFER_BASE_ADDR, buf_base, 2);
    if (err != SX1262_OK) return err;

    ENSURE(dev->spi != NULL);
    return SX1262_OK;
}

sx1262_err_t sx1262_set_frequency(sx1262_t *dev, uint32_t freq_hz)
{
    if (!dev || !dev->spi) return SX1262_ERR_PARAM;

    /* RF freq register = freq_hz * 2^25 / 32e6 */
    uint32_t frf = (uint32_t)(((uint64_t)freq_hz << 25) / 32000000UL);

    uint8_t params[4];
    params[0] = (uint8_t)(frf >> 24);
    params[1] = (uint8_t)(frf >> 16);
    params[2] = (uint8_t)(frf >>  8);
    params[3] = (uint8_t)(frf >>  0);

    sx1262_err_t err = send_command(dev->spi, CMD_SET_RF_FREQUENCY, params, 4);
    if (err != SX1262_OK) return err;

    dev->cfg.frequency_hz = freq_hz;
    return SX1262_OK;
}

sx1262_err_t sx1262_configure_lora(sx1262_t *dev, sx1262_sf_t sf,
                                   sx1262_bw_t bw, sx1262_cr_t cr)
{
    if (!dev || !dev->spi) return SX1262_ERR_PARAM;
    if (sf < SX1262_SF5 || sf > SX1262_SF12) return SX1262_ERR_PARAM;
    if (cr < SX1262_CR_4_5 || cr > SX1262_CR_4_8) return SX1262_ERR_PARAM;

    /* SetModulationParams: SF, BW, CR, LowDataRateOptimize */
    uint8_t ldro = (sf >= SX1262_SF11) ? 0x01 : 0x00;
    uint8_t mod_params[4];
    mod_params[0] = (uint8_t)sf;
    mod_params[1] = (uint8_t)bw;
    mod_params[2] = (uint8_t)cr;
    mod_params[3] = ldro;

    sx1262_err_t err = send_command(dev->spi, CMD_SET_MODULATION_PARAMS,
                                    mod_params, 4);
    if (err != SX1262_OK) return err;

    dev->cfg.spreading_factor = sf;
    dev->cfg.bandwidth = bw;
    dev->cfg.coding_rate = cr;
    return SX1262_OK;
}

sx1262_err_t sx1262_transmit(sx1262_t *dev, const uint8_t *data,
                             uint8_t len)
{
    if (!dev || !dev->spi) return SX1262_ERR_PARAM;
    if (!data || len == 0) return SX1262_ERR_PARAM;

    const sx1262_spi_t *spi = dev->spi;

    /* SetPacketParams: preamble=8, header=explicit(0), payload_len, CRC=on(1), InvertIQ=off(0) */
    uint8_t pkt_params[6];
    pkt_params[0] = 0x00; /* preamble MSB */
    pkt_params[1] = 0x08; /* preamble LSB = 8 symbols */
    pkt_params[2] = 0x00; /* explicit header */
    pkt_params[3] = len;  /* payload length */
    pkt_params[4] = 0x01; /* CRC on */
    pkt_params[5] = 0x00; /* standard IQ */

    sx1262_err_t err = send_command(spi, CMD_SET_PACKET_PARAMS,
                                    pkt_params, 6);
    if (err != SX1262_OK) return err;

    /* Write payload to buffer at offset 0 */
    err = write_buffer(spi, 0x00, data, len);
    if (err != SX1262_OK) return err;

    /* Clear IRQ */
    err = clear_irq(spi, IRQ_ALL);
    if (err != SX1262_OK) return err;

    /* SetTx(timeout=0 = no timeout from chip side, we poll) */
    uint8_t tx_tout[3] = {0x00, 0x00, 0x00};
    err = send_command(spi, CMD_SET_TX, tx_tout, 3);
    if (err != SX1262_OK) return err;

    /* Poll for TxDone */
    for (int i = 0; i < POLL_MAX; i++) {
        spi->delay_ms(spi->ctx, 10);
        uint16_t irq = 0;
        err = get_irq_status(spi, &irq);
        if (err != SX1262_OK) return err;

        if (irq & IRQ_TX_DONE) {
            (void)clear_irq(spi, IRQ_ALL);
            return SX1262_OK;
        }
    }

    /* Timeout -- go back to standby */
    uint8_t stdby = STDBY_RC;
    (void)send_command(spi, CMD_SET_STANDBY, &stdby, 1);
    return SX1262_ERR_TIMEOUT;
}

sx1262_err_t sx1262_receive(sx1262_t *dev, uint8_t *buf, uint8_t capacity,
                            uint8_t *out_len)
{
    if (!dev || !dev->spi) return SX1262_ERR_PARAM;
    if (!buf || !out_len) return SX1262_ERR_PARAM;

    const sx1262_spi_t *spi = dev->spi;

    /* SetPacketParams for RX: preamble=8, explicit header, max len, CRC on */
    uint8_t pkt_params[6];
    pkt_params[0] = 0x00;
    pkt_params[1] = 0x08;
    pkt_params[2] = 0x00; /* explicit header */
    pkt_params[3] = capacity;
    pkt_params[4] = 0x01; /* CRC on */
    pkt_params[5] = 0x00; /* standard IQ */

    sx1262_err_t err = send_command(spi, CMD_SET_PACKET_PARAMS,
                                    pkt_params, 6);
    if (err != SX1262_OK) return err;

    /* Clear IRQ */
    err = clear_irq(spi, IRQ_ALL);
    if (err != SX1262_OK) return err;

    /* SetRx(timeout=0 = continuous, we poll ourselves) */
    uint8_t rx_tout[3] = {0x00, 0x00, 0x00};
    err = send_command(spi, CMD_SET_RX, rx_tout, 3);
    if (err != SX1262_OK) return err;

    /* Poll for RxDone */
    for (int i = 0; i < POLL_MAX; i++) {
        spi->delay_ms(spi->ctx, 10);
        uint16_t irq = 0;
        err = get_irq_status(spi, &irq);
        if (err != SX1262_OK) return err;

        if (irq & IRQ_RX_DONE) {
            /* GetRxBufferStatus: payload_len, start_offset */
            uint8_t rx_status[2];
            err = read_command(spi, CMD_GET_RX_BUFFER_STATUS,
                               rx_status, 2);
            if (err != SX1262_OK) return err;

            uint8_t payload_len = rx_status[0];
            uint8_t start_offset = rx_status[1];

            if (payload_len > capacity) {
                payload_len = capacity;
            }

            err = read_buffer(spi, start_offset, buf, payload_len);
            if (err != SX1262_OK) return err;

            *out_len = payload_len;
            (void)clear_irq(spi, IRQ_ALL);
            return SX1262_OK;
        }
    }

    uint8_t stdby = STDBY_RC;
    (void)send_command(spi, CMD_SET_STANDBY, &stdby, 1);
    return SX1262_ERR_TIMEOUT;
}

sx1262_err_t sx1262_get_rssi(sx1262_t *dev, int16_t *rssi)
{
    if (!dev || !dev->spi || !rssi) return SX1262_ERR_PARAM;

    /* GetPacketStatus: 3 bytes for LoRa (rssiPkt, snrPkt, signalRssiPkt) */
    uint8_t pkt_status[3];
    sx1262_err_t err = read_command(dev->spi, CMD_GET_PACKET_STATUS,
                                    pkt_status, 3);
    if (err != SX1262_OK) return err;

    /* RSSI = -rssiPkt / 2 */
    *rssi = (int16_t)(-(int16_t)pkt_status[0] / 2);
    return SX1262_OK;
}

sx1262_err_t sx1262_sleep(sx1262_t *dev)
{
    if (!dev || !dev->spi) return SX1262_ERR_PARAM;
    /* SetSleep: sleepConfig = 0x00 (cold start, no RTC) */
    uint8_t param = 0x00;
    return send_command(dev->spi, CMD_SET_SLEEP, &param, 1);
}

sx1262_err_t sx1262_standby(sx1262_t *dev)
{
    if (!dev || !dev->spi) return SX1262_ERR_PARAM;
    uint8_t param = STDBY_RC;
    return send_command(dev->spi, CMD_SET_STANDBY, &param, 1);
}