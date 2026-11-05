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
#define CMD_GET_PACKET_STATUS     0x14U
#define CMD_WRITE_REGISTER        0x0DU
#define CMD_READ_REGISTER         0x1DU
#define CMD_SET_PA_CONFIG         0x95U
#define CMD_SET_BUFFER_BASE       0x8FU

/* Registers */
#define REG_TX_MODULATION         0x0889U
#define REG_TX_CLAMP_CONFIG       0x08D8U
#define REG_RTC_CTRL              0x0902U
#define REG_EVT_CLR               0x0944U

#define TX_MODULATION_BW_BIT      0x04U
#define TX_CLAMP_CONFIG_MASK      0x1EU
#define EVT_CLR_TIMEOUT           0x02U

/* IRQ bit masks */
#define IRQ_TX_DONE               0x0001U
#define IRQ_RX_DONE               0x0002U
#define IRQ_HEADER_ERROR          0x0020U
#define IRQ_CRC_ERROR             0x0040U
#define IRQ_TIMEOUT               0x0200U

/* Status byte: chip mode in bits 6:4 */
#define STATUS_MODE_SHIFT         4U
#define STATUS_MODE_MASK          0x07U
#define STATUS_MODE_STBY_RC       0x02U

/* Packet type */
#define PACKET_TYPE_LORA          0x01U

/* Standby config */
#define STDBY_RC                  0x00U

/* Packet format: 8 symbol preamble, explicit header, CRC on, standard IQ */
#define PREAMBLE_SYMBOLS          8U
#define PAYLOAD_MAX               255U

/* PA: full SX1262 high power path, power set separately with SetTxParams */
#define PA_DUTY_CYCLE             0x04U
#define PA_HP_MAX                 0x07U
#define PA_DEVICE_SX1262          0x00U
#define PA_LUT                    0x01U
#define TX_RAMP_200US             0x04U
#define TX_POWER_DEFAULT          14

/* Symbols of 16.38 ms and longer need low data rate optimization */
#define LDRO_SYMBOL_US            16380U

/* Frequency calculation: freq_reg = freq_hz * 2^25 / 32000000 */
#define FXOSC                     32000000UL

/* Polling limits */
#define BUSY_POLL_MAX             200U
#define POLL_STEP_MS              10U

/* Chip timeouts count RTC steps of 1/64 ms */
#define RTC_STEPS_PER_MS          64U
#define RTC_TIMEOUT_MAX           0xFFFFFEUL
#define RX_TIMEOUT_MS             3000U
#define TIMEOUT_MARGIN_MS         1000U

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

static bool spi_failed(const sx1262_hal_t *hal)
{
    return hal->get_spi_error && hal->get_spi_error(hal->ctx);
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

    if (spi_failed(hal)) {
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

    if (spi_failed(hal)) {
        return SX1262_ERR_SPI;
    }

    if (result && result_len > 0) {
        memcpy(result, &rx[2], result_len);
    }

    return SX1262_OK;
}

static sx1262_err_t write_register(const sx1262_hal_t *hal, uint16_t addr,
                                   uint8_t value)
{
    uint8_t params[3];

    params[0] = (uint8_t)(addr >> 8);
    params[1] = (uint8_t)(addr & 0xFFU);
    params[2] = value;
    return send_command(hal, CMD_WRITE_REGISTER, params, sizeof(params));
}

static sx1262_err_t read_register(const sx1262_hal_t *hal, uint16_t addr,
                                  uint8_t *value)
{
    uint8_t tx[5] = { CMD_READ_REGISTER, 0, 0, 0, 0 };
    uint8_t rx[5];
    sx1262_err_t err;

    tx[1] = (uint8_t)(addr >> 8);
    tx[2] = (uint8_t)(addr & 0xFFU);

    err = wait_busy(hal);
    if (err != SX1262_OK) return err;

    hal->cs_select(hal->ctx);
    hal->transfer(hal->ctx, tx, rx, sizeof(tx));
    hal->cs_deselect(hal->ctx);

    if (spi_failed(hal)) {
        return SX1262_ERR_SPI;
    }

    *value = rx[4];
    return SX1262_OK;
}

static sx1262_err_t read_status(const sx1262_hal_t *hal, uint8_t *status)
{
    uint8_t tx[2] = { CMD_GET_STATUS, 0x00 };
    uint8_t rx[2];
    sx1262_err_t err = wait_busy(hal);
    if (err != SX1262_OK) return err;

    hal->cs_select(hal->ctx);
    hal->transfer(hal->ctx, tx, rx, sizeof(tx));
    hal->cs_deselect(hal->ctx);

    if (spi_failed(hal)) {
        return SX1262_ERR_SPI;
    }

    *status = rx[1];
    return SX1262_OK;
}

static uint32_t bandwidth_hz(sx1262_bw_t bw)
{
    switch (bw) {
    case SX1262_BW_7K8:   return 7812U;
    case SX1262_BW_10K4:  return 10417U;
    case SX1262_BW_15K6:  return 15625U;
    case SX1262_BW_20K8:  return 20833U;
    case SX1262_BW_31K25: return 31250U;
    case SX1262_BW_41K7:  return 41667U;
    case SX1262_BW_62K5:  return 62500U;
    case SX1262_BW_125K:  return 125000U;
    case SX1262_BW_250K:  return 250000U;
    case SX1262_BW_500K:  return 500000U;
    default:              return 0U;
    }
}

static bool needs_ldro(sx1262_sf_t sf, sx1262_bw_t bw)
{
    uint64_t symbol_us = ((uint64_t)1000000U << (unsigned)sf) / bandwidth_hz(bw);
    return symbol_us >= LDRO_SYMBOL_US;
}

/* LoRa time on air, from the formula in Semtech's reference driver. */
static uint32_t time_on_air_ms(const sx1262_t *dev, uint8_t payload_len)
{
    int32_t sf = (int32_t)dev->sf;
    int32_t cr_denominator = (int32_t)dev->cr + 4;
    int32_t numerator = ((int32_t)payload_len * 8) + 16 - (4 * sf) + 20;
    int32_t denominator = 4 * sf;

    if (sf > 6) {
        numerator += 8;
        if (needs_ldro(dev->sf, dev->bw)) {
            denominator = 4 * (sf - 2);
        }
    }
    if (numerator < 0) {
        numerator = 0;
    }

    int32_t symbols = ((numerator + denominator - 1) / denominator) * cr_denominator +
                      (int32_t)PREAMBLE_SYMBOLS;
    if (sf <= 6) {
        symbols += 2;
    }

    uint64_t quarter_symbols = (uint64_t)(4 * symbols + 1) << (uint32_t)(sf - 2);
    uint32_t bw = bandwidth_hz(dev->bw);
    return (uint32_t)((quarter_symbols * 1000U + bw - 1U) / bw);
}

static void rtc_timeout(uint32_t timeout_ms, uint8_t params[3])
{
    uint64_t steps = (uint64_t)timeout_ms * RTC_STEPS_PER_MS;

    if (steps > RTC_TIMEOUT_MAX) {
        steps = RTC_TIMEOUT_MAX;
    }
    params[0] = (uint8_t)(steps >> 16);
    params[1] = (uint8_t)(steps >> 8);
    params[2] = (uint8_t)steps;
}

static sx1262_err_t set_packet_params(const sx1262_t *dev, uint8_t payload_len)
{
    uint8_t params[6];

    params[0] = 0x00;             /* preamble MSB */
    params[1] = PREAMBLE_SYMBOLS; /* preamble LSB */
    params[2] = 0x00;             /* explicit header */
    params[3] = payload_len;
    params[4] = 0x01;             /* CRC on */
    params[5] = 0x00;             /* standard IQ */
    return send_command(dev->hal, CMD_SET_PACKET_PARAMS, params, sizeof(params));
}

/* Antenna mismatch workaround: raises the PA clamping threshold. */
static sx1262_err_t apply_tx_clamp(const sx1262_hal_t *hal)
{
    uint8_t value = 0;
    sx1262_err_t err = read_register(hal, REG_TX_CLAMP_CONFIG, &value);
    if (err != SX1262_OK) return err;

    return write_register(hal, REG_TX_CLAMP_CONFIG,
                          (uint8_t)(value | TX_CLAMP_CONFIG_MASK));
}

/* Modulation quality workaround: this bit is clear only at 500 kHz. */
static sx1262_err_t apply_tx_modulation(const sx1262_t *dev)
{
    uint8_t value = 0;
    sx1262_err_t err = read_register(dev->hal, REG_TX_MODULATION, &value);
    if (err != SX1262_OK) return err;

    if (dev->bw == SX1262_BW_500K) {
        value = (uint8_t)(value & (uint8_t)~TX_MODULATION_BW_BIT);
    } else {
        value = (uint8_t)(value | TX_MODULATION_BW_BIT);
    }
    return write_register(dev->hal, REG_TX_MODULATION, value);
}

/* Required after a reception with a timeout: stop the RTC and clear its event. */
static sx1262_err_t stop_rx_timer(const sx1262_hal_t *hal)
{
    uint8_t value = 0;
    sx1262_err_t err = write_register(hal, REG_RTC_CTRL, 0x00);
    if (err != SX1262_OK) return err;

    err = read_register(hal, REG_EVT_CLR, &value);
    if (err != SX1262_OK) return err;

    return write_register(hal, REG_EVT_CLR, (uint8_t)(value | EVT_CLR_TIMEOUT));
}

static sx1262_err_t clear_irq(const sx1262_hal_t *hal)
{
    static const uint8_t all[2] = { 0xFF, 0xFF };
    return send_command(hal, CMD_CLEAR_IRQ_STATUS, all, sizeof(all));
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
    dev->tx_power = TX_POWER_DEFAULT;

    /* enter standby (RC) */
    param = STDBY_RC;
    err = send_command(hal, CMD_SET_STANDBY, &param, 1);
    if (err != SX1262_OK) return err;

    hal->delay_ms(hal->ctx, 5);

    /* The chip has to report standby, or nothing answered on the bus. */
    err = read_status(hal, &param);
    if (err != SX1262_OK) return err;
    if (((param >> STATUS_MODE_SHIFT) & STATUS_MODE_MASK) != STATUS_MODE_STBY_RC) {
        return SX1262_ERR_SPI;
    }

    /* set packet type to LoRa */
    param = PACKET_TYPE_LORA;
    err = send_command(hal, CMD_SET_PACKET_TYPE, &param, 1);
    if (err != SX1262_OK) return err;

    /* TX and RX both start at the beginning of the data buffer */
    static const uint8_t buffer_base[2] = { 0x00, 0x00 };
    err = send_command(hal, CMD_SET_BUFFER_BASE, buffer_base, sizeof(buffer_base));
    if (err != SX1262_OK) return err;

    err = apply_tx_clamp(hal);
    if (err != SX1262_OK) return err;

    static const uint8_t pa_config[4] = {
        PA_DUTY_CYCLE, PA_HP_MAX, PA_DEVICE_SX1262, PA_LUT
    };
    err = send_command(hal, CMD_SET_PA_CONFIG, pa_config, sizeof(pa_config));
    if (err != SX1262_OK) return err;

    const uint8_t tx_params[2] = { (uint8_t)dev->tx_power, TX_RAMP_200US };
    return send_command(hal, CMD_SET_TX_PARAMS, tx_params, sizeof(tx_params));
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
    sx1262_err_t err;

    if (!dev || !dev->hal) return SX1262_ERR_PARAM;
    if (sf < SX1262_SF5 || sf > SX1262_SF12) return SX1262_ERR_PARAM;
    if (bandwidth_hz(bw) == 0U) return SX1262_ERR_PARAM;
    if (cr < SX1262_CR_4_5 || cr > SX1262_CR_4_8) return SX1262_ERR_PARAM;

    /* SetModulationParams: SF, BW, CR, LowDataRateOptimize */
    mod_params[0] = (uint8_t)sf;
    mod_params[1] = (uint8_t)bw;
    mod_params[2] = (uint8_t)cr;
    mod_params[3] = needs_ldro(sf, bw) ? 0x01U : 0x00U;

    err = send_command(dev->hal, CMD_SET_MOD_PARAMS, mod_params, 4);
    if (err != SX1262_OK) return err;

    err = set_packet_params(dev, PAYLOAD_MAX);
    if (err != SX1262_OK) return err;

    dev->sf = sf;
    dev->bw = bw;
    dev->cr = cr;
    return apply_tx_modulation(dev);
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

    /* The chip sends the length set here, not what the buffer write held. */
    err = set_packet_params(dev, len);
    if (err != SX1262_OK) return err;

    /* WriteBuffer: opcode + offset(0) + data */
    buf[0] = CMD_WRITE_BUFFER;
    buf[1] = 0x00; /* offset */
    memcpy(&buf[2], data, len);

    err = wait_busy(dev->hal);
    if (err != SX1262_OK) return err;

    dev->hal->cs_select(dev->hal->ctx);
    dev->hal->transfer(dev->hal->ctx, buf, NULL, (size_t)len + 2);
    dev->hal->cs_deselect(dev->hal->ctx);

    if (spi_failed(dev->hal)) {
        return SX1262_ERR_SPI;
    }

    /* SetDioIrqParams: enable TxDone + Timeout */
    memset(irq_params, 0, sizeof(irq_params));
    irq_params[0] = (uint8_t)((IRQ_TX_DONE | IRQ_TIMEOUT) >> 8);
    irq_params[1] = (uint8_t)(IRQ_TX_DONE | IRQ_TIMEOUT);
    irq_params[2] = irq_params[0]; /* DIO1 mask MSB */
    irq_params[3] = irq_params[1]; /* DIO1 mask LSB */
    err = send_command(dev->hal, CMD_SET_DIO_IRQ_PARAMS, irq_params, 8);
    if (err != SX1262_OK) return err;

    err = clear_irq(dev->hal);
    if (err != SX1262_OK) return err;

    /* SetTx: time on air plus a margin, since a long packet takes minutes */
    uint32_t timeout_ms = time_on_air_ms(dev, len) + TIMEOUT_MARGIN_MS;
    rtc_timeout(timeout_ms, tx_params);
    err = send_command(dev->hal, CMD_SET_TX, tx_params, 3);
    if (err != SX1262_OK) return err;

    /* Poll for TxDone, for longer than the chip's own timeout */
    uint32_t polls = (timeout_ms + TIMEOUT_MARGIN_MS) / POLL_STEP_MS;
    for (i = 0; i < polls; i++) {
        dev->hal->delay_ms(dev->hal->ctx, POLL_STEP_MS);

        err = read_command(dev->hal, CMD_GET_IRQ_STATUS, irq_status, 2);
        if (err != SX1262_OK) return err;

        uint16_t irq = (uint16_t)(((uint16_t)irq_status[0] << 8) |
                                  (uint16_t)irq_status[1]);
        if (irq & IRQ_TX_DONE) {
            (void)clear_irq(dev->hal);
            return SX1262_OK;
        }
        if (irq & IRQ_TIMEOUT) {
            (void)clear_irq(dev->hal);
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
    sx1262_err_t err;
    uint32_t i;
    uint8_t payload_len;
    uint8_t rx_offset;

    if (!dev || !dev->hal) return SX1262_ERR_PARAM;
    if (!buf || !out_len || cap == 0) return SX1262_ERR_PARAM;

    /* Explicit header: the length comes from the packet, up to this limit */
    err = set_packet_params(dev, PAYLOAD_MAX);
    if (err != SX1262_OK) return err;

    /* SetDioIrqParams: enable RxDone, Timeout and both error flags */
    const uint16_t irq_mask = IRQ_RX_DONE | IRQ_TIMEOUT | IRQ_HEADER_ERROR | IRQ_CRC_ERROR;
    memset(irq_params, 0, sizeof(irq_params));
    irq_params[0] = (uint8_t)(irq_mask >> 8);
    irq_params[1] = (uint8_t)irq_mask;
    irq_params[2] = irq_params[0]; /* DIO1 mask MSB */
    irq_params[3] = irq_params[1]; /* DIO1 mask LSB */
    err = send_command(dev->hal, CMD_SET_DIO_IRQ_PARAMS, irq_params, 8);
    if (err != SX1262_OK) return err;

    err = clear_irq(dev->hal);
    if (err != SX1262_OK) return err;

    /* SetRx: wait up to 3 s for a packet to start */
    rtc_timeout(RX_TIMEOUT_MS, rx_params);
    err = send_command(dev->hal, CMD_SET_RX, rx_params, 3);
    if (err != SX1262_OK) return err;

    /* Poll for RxDone: a packet starting at the deadline still has to finish */
    uint32_t polls = (RX_TIMEOUT_MS + time_on_air_ms(dev, PAYLOAD_MAX) +
                      TIMEOUT_MARGIN_MS) / POLL_STEP_MS;
    for (i = 0; i < polls; i++) {
        dev->hal->delay_ms(dev->hal->ctx, POLL_STEP_MS);

        err = read_command(dev->hal, CMD_GET_IRQ_STATUS, irq_status, 2);
        if (err != SX1262_OK) return err;

        uint16_t irq = (uint16_t)(((uint16_t)irq_status[0] << 8) |
                                  (uint16_t)irq_status[1]);
        if (irq & (IRQ_CRC_ERROR | IRQ_HEADER_ERROR)) {
            /* RxDone fires for a bad packet as well, so check these first */
            (void)clear_irq(dev->hal);
            (void)stop_rx_timer(dev->hal);
            return SX1262_ERR_CRC;
        }
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

                if (spi_failed(dev->hal)) {
                    return SX1262_ERR_SPI;
                }

                memcpy(buf, &rd_rx[3], payload_len);
            }

            *out_len = payload_len;

            (void)clear_irq(dev->hal);
            (void)stop_rx_timer(dev->hal);
            return SX1262_OK;
        }
        if (irq & IRQ_TIMEOUT) {
            (void)clear_irq(dev->hal);
            (void)stop_rx_timer(dev->hal);
            return SX1262_ERR_TIMEOUT;
        }
    }

    return SX1262_ERR_TIMEOUT;
}

int16_t sx1262_get_rssi(sx1262_t *dev)
{
    uint8_t status[3]; /* RssiPkt, SnrPkt, SignalRssiPkt */
    sx1262_err_t err;

    if (!dev || !dev->hal) return 0;

    err = read_command(dev->hal, CMD_GET_PACKET_STATUS, status, sizeof(status));
    if (err != SX1262_OK) return 0;

    /* RSSI [dBm] = -RssiPkt / 2, rounded down as the reference driver does */
    return (int16_t)(-(((int16_t)status[0] + 1) / 2));
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
    uint8_t status = 0;

    if (!dev || !dev->hal) return 0;

    if (read_status(dev->hal, &status) != SX1262_OK) return 0;

    return status;
}
