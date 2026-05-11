// SPDX-License-Identifier: MIT
/*
 * sx1262 — SPI LoRa radio transceiver driver for Semtech SX1262
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "sx1262.h"
#include "contract.h"
#include <string.h>

/* ---------- SX1262 opcodes ---------- */

#define CMD_SET_SLEEP             0x84U
#define CMD_SET_STANDBY           0x80U
#define CMD_SET_PACKET_TYPE       0x8AU
#define CMD_SET_RF_FREQUENCY      0x86U
#define CMD_SET_TX_PARAMS         0x8EU
#define CMD_SET_MOD_PARAMS        0x8BU
#define CMD_SET_PACKET_PARAMS     0x8CU
#define CMD_WRITE_BUFFER          0x0EU
#define CMD_READ_BUFFER           0x1EU
#define CMD_GET_STATUS            0xC0U
#define CMD_SET_DIO_IRQ_PARAMS    0x08U
#define CMD_GET_IRQ_STATUS        0x12U
#define CMD_CLEAR_IRQ_STATUS      0x02U
#define CMD_SET_TX                0x83U
#define CMD_SET_RX                0x82U
#define CMD_GET_RX_BUFFER_STATUS  0x13U
#define CMD_GET_RSSI_INST         0x15U

/* IRQ bit masks */
#define IRQ_TX_DONE               0x0001U
#define IRQ_RX_DONE               0x0002U
#define IRQ_TIMEOUT               0x0200U

/* Packet type */
#define PACKET_TYPE_LORA          0x01U

/* Standby config */
#define STDBY_RC                  0x00U

/* Frequency calculation: freq_reg = freq_hz * 2^25 / 32000000 */
#define FXOSC                     32000000UL

/* Polling limits */
#define BUSY_POLL_MAX             200U
#define TXRX_POLL_MAX             200U

/* ---------- internal helpers ---------- */

static sx1262_err_t wait_busy(const sx1262_hal_t *hal)
{
    uint32_t i;
    for (i = 0; i < BUSY_POLL_MAX; i++) {
        if (!hal->is_busy(hal->ctx)) {
            return SX1262_OK;
        }
        hal->delay_ms(hal->ctx, 1);
    }
    return SX1262_ERR_BUSY;
}

static sx1262_err_t send_command(const sx1262_hal_t *hal,
                                 uint8_t opcode,
                                 const uint8_t *params,
                                 size_t param_len)
{
    uint8_t buf[16];
    sx1262_err_t err;

    REQUIRE(param_len + 1 <= sizeof(buf));

    err = wait_busy(hal);
    if (err != SX1262_OK) return err;

    buf[0] = opcode;
    if (params && param_len > 0) {
        memcpy(&buf[1], params, param_len);
    }

    hal->cs_select(hal->ctx);
    hal->transfer(hal->ctx, buf, NULL, param_len + 1);
    hal->cs_deselect(hal->ctx);

    if (hal->get_spi_error && hal->get_spi_error(hal->ctx)) {
        return SX1262_ERR_SPI;
    }

    return SX1262_OK;
}

static sx1262_err_t read_command(const sx1262_hal_t *hal,
                                 uint8_t opcode,
                                 uint8_t *result,
                                 size_t result_len)
{
    uint8_t tx[16];
    uint8_t rx[16];
    sx1262_err_t err;
    size_t total = 1 + 1 + result_len; /* opcode + NOP + data */

    REQUIRE(total <= sizeof(tx));

    err = wait_busy(hal);
    if (err != SX1262_OK) return err;

    memset(tx, 0x00, total);
    tx[0] = opcode;

    hal->cs_select(hal->ctx);
    hal->transfer(hal->ctx, tx, rx, total);
    hal->cs_deselect(hal->ctx);

    if (result && result_len > 0) {
        memcpy(result, &rx[2], result_len);
    }

    return SX1262_OK;
}

/* ---------- public API ---------- */

sx1262_err_t sx1262_init(sx1262_t *dev, const sx1262_hal_t *hal)
{
    sx1262_err_t err;
    uint8_t param;

    if (!dev || !hal) return SX1262_ERR_PARAM;
    if (!hal->cs_select || !hal->cs_deselect || !hal->transfer ||
        !hal->is_busy || !hal->delay_ms) {
        return SX1262_ERR_PARAM;
    }

    dev->hal = hal;
    dev->frequency = 0;
    dev->sf = SX1262_SF7;
    dev->bw = SX1262_BW_125K;
    dev->cr = SX1262_CR_4_5;
    dev->tx_power = 14;

    /* enter standby (RC) */
    param = STDBY_RC;
    err = send_command(hal, CMD_SET_STANDBY, &param, 1);
    if (err != SX1262_OK) return err;

    hal->delay_ms(hal->ctx, 5);

    /* set packet type to LoRa */
    param = PACKET_TYPE_LORA;
    err = send_command(hal, CMD_SET_PACKET_TYPE, &param, 1);
    if (err != SX1262_OK) return err;

    return SX1262_OK;
}

sx1262_err_t sx1262_set_frequency(sx1262_t *dev, uint32_t freq_hz)
{
    uint8_t params[4];
    uint32_t freq_reg;
    sx1262_err_t err;

    if (!dev || !dev->hal) return SX1262_ERR_PARAM;
    if (freq_hz < 150000000UL || freq_hz > 960000000UL) {
        return SX1262_ERR_PARAM;
    }

    /* freq_reg = freq_hz * 2^25 / 32000000 */
    freq_reg = (uint32_t)(((uint64_t)freq_hz << 25) / FXOSC);

    params[0] = (uint8_t)(freq_reg >> 24);
    params[1] = (uint8_t)(freq_reg >> 16);
    params[2] = (uint8_t)(freq_reg >>  8);
    params[3] = (uint8_t)(freq_reg >>  0);

    err = send_command(dev->hal, CMD_SET_RF_FREQUENCY, params, 4);
    if (err != SX1262_OK) return err;

    dev->frequency = freq_hz;
    return SX1262_OK;
}

sx1262_err_t sx1262_configure_lora(sx1262_t *dev, sx1262_sf_t sf,
                                   sx1262_bw_t bw, sx1262_cr_t cr)
{
    uint8_t mod_params[4];
    uint8_t pkt_params[6];
    sx1262_err_t err;

    if (!dev || !dev->hal) return SX1262_ERR_PARAM;
    if (sf < SX1262_SF5 || sf > SX1262_SF12) return SX1262_ERR_PARAM;
    if (cr < SX1262_CR_4_5 || cr > SX1262_CR_4_8) return SX1262_ERR_PARAM;

    /* SetModulationParams: SF, BW, CR, LowDataRateOptimize */
    mod_params[0] = (uint8_t)sf;
    mod_params[1] = (uint8_t)bw;
    mod_params[2] = (uint8_t)cr;
    mod_params[3] = 0x00; /* LDRO off */

    err = send_command(dev->hal, CMD_SET_MOD_PARAMS, mod_params, 4);
    if (err != SX1262_OK) return err;

    /* SetPacketParams: preamble=8, header=explicit, payload=255, CRC=on, IQ=standard */
    pkt_params[0] = 0x00; /* preamble MSB */
    pkt_params[1] = 0x08; /* preamble LSB = 8 symbols */
    pkt_params[2] = 0x00; /* explicit header */
    pkt_params[3] = 0xFF; /* max payload 255 */
    pkt_params[4] = 0x01; /* CRC on */
    pkt_params[5] = 0x00; /* standard IQ */

    err = send_command(dev->hal, CMD_SET_PACKET_PARAMS, pkt_params, 6);
    if (err != SX1262_OK) return err;

    dev->sf = sf;
    dev->bw = bw;
    dev->cr = cr;
    return SX1262_OK;
}

sx1262_err_t sx1262_transmit(sx1262_t *dev, const uint8_t *data, uint8_t len)
{
    uint8_t buf[258]; /* 1 offset + 1 opcode overhead + 255 data */
    uint8_t irq_params[8];
    uint8_t irq_status[2];
    uint8_t tx_params[3];
    sx1262_err_t err;
    uint32_t i;

    if (!dev || !dev->hal) return SX1262_ERR_PARAM;
    if (!data || len == 0) return SX1262_ERR_PARAM;

    /* WriteBuffer: opcode + offset(0) + data */
    buf[0] = CMD_WRITE_BUFFER;
    buf[1] = 0x00; /* offset */
    memcpy(&buf[2], data, len);

    err = wait_busy(dev->hal);
    if (err != SX1262_OK) return err;

    dev->hal->cs_select(dev->hal->ctx);
    dev->hal->transfer(dev->hal->ctx, buf, NULL, (size_t)len + 2);
    dev->hal->cs_deselect(dev->hal->ctx);

    /* SetDioIrqParams: enable TxDone */
    memset(irq_params, 0, sizeof(irq_params));
    irq_params[0] = 0x00; /* IRQ mask MSB */
    irq_params[1] = 0x01; /* IRQ mask LSB: TxDone */
    irq_params[2] = 0x00; /* DIO1 mask MSB */
    irq_params[3] = 0x01; /* DIO1 mask LSB */
    err = send_command(dev->hal, CMD_SET_DIO_IRQ_PARAMS, irq_params, 8);
    if (err != SX1262_OK) return err;

    /* Clear IRQ status */
    buf[0] = 0xFF;
    buf[1] = 0xFF;
    err = send_command(dev->hal, CMD_CLEAR_IRQ_STATUS, buf, 2);
    if (err != SX1262_OK) return err;

    /* SetTx: timeout = 3s (0x004B00 in 15.625us steps) */
    tx_params[0] = 0x00;
    tx_params[1] = 0x4B;
    tx_params[2] = 0x00;
    err = send_command(dev->hal, CMD_SET_TX, tx_params, 3);
    if (err != SX1262_OK) return err;

    /* Poll for TxDone */
    for (i = 0; i < TXRX_POLL_MAX; i++) {
        dev->hal->delay_ms(dev->hal->ctx, 10);

        err = read_command(dev->hal, CMD_GET_IRQ_STATUS, irq_status, 2);
        if (err != SX1262_OK) return err;

        uint16_t irq = (uint16_t)(((uint16_t)irq_status[0] << 8) |
                                  (uint16_t)irq_status[1]);
        if (irq & IRQ_TX_DONE) {
            /* Clear IRQ */
            buf[0] = 0xFF;
            buf[1] = 0xFF;
            (void)send_command(dev->hal, CMD_CLEAR_IRQ_STATUS, buf, 2);
            return SX1262_OK;
        }
        if (irq & IRQ_TIMEOUT) {
            buf[0] = 0xFF;
            buf[1] = 0xFF;
            (void)send_command(dev->hal, CMD_CLEAR_IRQ_STATUS, buf, 2);
            return SX1262_ERR_TIMEOUT;
        }
    }

    return SX1262_ERR_TIMEOUT;
}

sx1262_err_t sx1262_receive(sx1262_t *dev, uint8_t *buf, uint8_t cap,
                            uint8_t *out_len)
{
    uint8_t irq_params[8];
    uint8_t irq_status[2];
    uint8_t rx_params[3];
    uint8_t rx_buf_status[2];
    uint8_t tmp[2];
    sx1262_err_t err;
    uint32_t i;
    uint8_t payload_len;
    uint8_t rx_offset;

    if (!dev || !dev->hal) return SX1262_ERR_PARAM;
    if (!buf || !out_len || cap == 0) return SX1262_ERR_PARAM;

    /* SetDioIrqParams: enable RxDone + Timeout */
    memset(irq_params, 0, sizeof(irq_params));
    irq_params[0] = 0x02; /* IRQ mask MSB: Timeout */
    irq_params[1] = 0x02; /* IRQ mask LSB: RxDone */
    irq_params[2] = 0x02; /* DIO1 mask MSB */
    irq_params[3] = 0x02; /* DIO1 mask LSB */
    err = send_command(dev->hal, CMD_SET_DIO_IRQ_PARAMS, irq_params, 8);
    if (err != SX1262_OK) return err;

    /* Clear IRQ status */
    tmp[0] = 0xFF;
    tmp[1] = 0xFF;
    err = send_command(dev->hal, CMD_CLEAR_IRQ_STATUS, tmp, 2);
    if (err != SX1262_OK) return err;

    /* SetRx: timeout = 3s */
    rx_params[0] = 0x00;
    rx_params[1] = 0x4B;
    rx_params[2] = 0x00;
    err = send_command(dev->hal, CMD_SET_RX, rx_params, 3);
    if (err != SX1262_OK) return err;

    /* Poll for RxDone */
    for (i = 0; i < TXRX_POLL_MAX; i++) {
        dev->hal->delay_ms(dev->hal->ctx, 10);

        err = read_command(dev->hal, CMD_GET_IRQ_STATUS, irq_status, 2);
        if (err != SX1262_OK) return err;

        uint16_t irq = (uint16_t)(((uint16_t)irq_status[0] << 8) |
                                  (uint16_t)irq_status[1]);
        if (irq & IRQ_RX_DONE) {
            /* GetRxBufferStatus: returns payload_len, rx_start_offset */
            err = read_command(dev->hal, CMD_GET_RX_BUFFER_STATUS,
                               rx_buf_status, 2);
            if (err != SX1262_OK) return err;

            payload_len = rx_buf_status[0];
            rx_offset   = rx_buf_status[1];

            if (payload_len > cap) {
                payload_len = cap;
            }

            /* ReadBuffer: opcode + offset + NOP + data */
            {
                uint8_t rd_tx[258];
                uint8_t rd_rx[258];
                size_t total = (size_t)payload_len + 3;
                memset(rd_tx, 0, total);
                rd_tx[0] = CMD_READ_BUFFER;
                rd_tx[1] = rx_offset;
                /* byte 2 is NOP */

                err = wait_busy(dev->hal);
                if (err != SX1262_OK) return err;

                dev->hal->cs_select(dev->hal->ctx);
                dev->hal->transfer(dev->hal->ctx, rd_tx, rd_rx, total);
                dev->hal->cs_deselect(dev->hal->ctx);

                memcpy(buf, &rd_rx[3], payload_len);
            }

            *out_len = payload_len;

            /* Clear IRQ */
            tmp[0] = 0xFF;
            tmp[1] = 0xFF;
            (void)send_command(dev->hal, CMD_CLEAR_IRQ_STATUS, tmp, 2);
            return SX1262_OK;
        }
        if (irq & IRQ_TIMEOUT) {
            tmp[0] = 0xFF;
            tmp[1] = 0xFF;
            (void)send_command(dev->hal, CMD_CLEAR_IRQ_STATUS, tmp, 2);
            return SX1262_ERR_TIMEOUT;
        }
    }

    return SX1262_ERR_TIMEOUT;
}

int16_t sx1262_get_rssi(sx1262_t *dev)
{
    uint8_t raw;
    sx1262_err_t err;

    if (!dev || !dev->hal) return 0;

    err = read_command(dev->hal, CMD_GET_RSSI_INST, &raw, 1);
    if (err != SX1262_OK) return 0;

    /* RSSI [dBm] = -raw / 2 */
    return (int16_t)(-(int16_t)raw / 2);
}

sx1262_err_t sx1262_sleep(sx1262_t *dev)
{
    uint8_t param;

    if (!dev || !dev->hal) return SX1262_ERR_PARAM;

    /* SetSleep: warm start (retain config) */
    param = 0x04;
    return send_command(dev->hal, CMD_SET_SLEEP, &param, 1);
}

sx1262_err_t sx1262_standby(sx1262_t *dev)
{
    uint8_t param;

    if (!dev || !dev->hal) return SX1262_ERR_PARAM;

    param = STDBY_RC;
    return send_command(dev->hal, CMD_SET_STANDBY, &param, 1);
}

uint8_t sx1262_get_status(sx1262_t *dev)
{
    uint8_t tx[2];
    uint8_t rx[2];
    sx1262_err_t err;

    if (!dev || !dev->hal) return 0;

    err = wait_busy(dev->hal);
    if (err != SX1262_OK) return 0;

    tx[0] = CMD_GET_STATUS;
    tx[1] = 0x00;

    dev->hal->cs_select(dev->hal->ctx);
    dev->hal->transfer(dev->hal->ctx, tx, rx, 2);
    dev->hal->cs_deselect(dev->hal->ctx);

    return rx[1];
}
