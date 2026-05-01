/*
 * lora_sx1276 — SPI LoRa radio transceiver driver for SX1276/SX1278
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:    spi_hal
 */

#include "lora_sx1276.h"

/* ---------- SX1276 register map ---------- */

#define REG_FIFO              0x00
#define REG_OP_MODE           0x01
#define REG_FRF_MSB           0x06
#define REG_FRF_MID           0x07
#define REG_FRF_LSB           0x08
#define REG_PA_CONFIG         0x09
#define REG_PA_DAC            0x4D
#define REG_FIFO_ADDR_PTR    0x0D
#define REG_FIFO_TX_BASE     0x0E
#define REG_FIFO_RX_BASE     0x0F
#define REG_FIFO_RX_CURRENT  0x10
#define REG_IRQ_FLAGS         0x12
#define REG_RX_NB_BYTES      0x13
#define REG_PKT_SNR           0x19
#define REG_PKT_RSSI          0x1A
#define REG_MODEM_CONFIG_1   0x1D
#define REG_MODEM_CONFIG_2   0x1E
#define REG_PAYLOAD_LEN      0x22
#define REG_MODEM_CONFIG_3   0x26
#define REG_VERSION           0x42

/* SPI write flag */
#define SPI_WRITE             0x80

/* OpMode bits */
#define OPMODE_LORA           0x80
#define MODE_SLEEP            0x00
#define MODE_STANDBY          0x01
#define MODE_TX               0x03
#define MODE_RX_SINGLE        0x06

/* IRQ flags */
#define IRQ_TX_DONE           0x08
#define IRQ_RX_DONE           0x40
#define IRQ_PAYLOAD_CRC_ERR  0x20

/* Version */
#define SX1276_VERSION        0x12

/* PA_DAC values */
#define PA_DAC_DEFAULT        0x84
#define PA_DAC_HIGH_POWER     0x87

/* Oscillator frequency for Frf calculation */
#define FXOSC                 32000000UL

/* Max polling iterations for TX/RX done */
#define POLL_MAX              200

/* ---------- internal helpers ---------- */

static uint8_t reg_read(const lora_spi_t *spi, uint8_t addr)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, addr & 0x7F);
    uint8_t val = spi->xfer(spi->ctx, 0x00);
    spi->cs_high(spi->ctx);
    return val;
}

static void reg_write(const lora_spi_t *spi, uint8_t addr, uint8_t val)
{
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, addr | SPI_WRITE);
    spi->xfer(spi->ctx, val);
    spi->cs_high(spi->ctx);
}

static void set_mode(const lora_spi_t *spi, uint8_t mode)
{
    reg_write(spi, REG_OP_MODE, OPMODE_LORA | mode);
}

/* ---------- public API ---------- */

lora_err_t lora_init(lora_t *dev, const lora_spi_t *spi, uint32_t freq_hz)
{
    if (!dev || !spi || !spi->cs_low || !spi->cs_high ||
        !spi->xfer || !spi->reset || !spi->delay_ms) {
        return LORA_ERR_SPI;
    }

    dev->spi = spi;
    dev->frequency = 0;
    dev->bandwidth = 7;        /* 125 kHz default */
    dev->spreading_factor = 7; /* SF7 default */
    dev->coding_rate = 1;      /* 4/5 default */
    dev->tx_power = 17;        /* max PA_BOOST default */

    /* hardware reset */
    spi->reset(spi->ctx);
    spi->delay_ms(spi->ctx, 10);

    /* enter sleep to switch to LoRa mode */
    set_mode(spi, MODE_SLEEP);
    spi->delay_ms(spi->ctx, 10);

    /* verify chip version */
    uint8_t ver = reg_read(spi, REG_VERSION);
    if (ver != SX1276_VERSION) {
        return LORA_ERR_VERSION;
    }

    /* set frequency */
    lora_err_t err = lora_set_frequency(dev, freq_hz);
    if (err != LORA_OK) return err;

    /* FIFO base addresses: TX=0x00, RX=0x00 (use full 256-byte FIFO) */
    reg_write(spi, REG_FIFO_TX_BASE, 0x00);
    reg_write(spi, REG_FIFO_RX_BASE, 0x00);

    /* LNA boost (max gain) + AGC auto on */
    reg_write(spi, REG_MODEM_CONFIG_3, 0x04);

    /* default TX power */
    err = lora_set_tx_power(dev, dev->tx_power);
    if (err != LORA_OK) return err;

    /* standby mode */
    set_mode(spi, MODE_STANDBY);

    return LORA_OK;
}

lora_err_t lora_set_frequency(lora_t *dev, uint32_t freq_hz)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (freq_hz < 137000000UL || freq_hz > 1020000000UL) {
        return LORA_ERR_PARAMS;
    }

    /* Frf = freq * 2^19 / Fxosc */
    uint64_t frf = ((uint64_t)freq_hz << 19) / FXOSC;

    const lora_spi_t *spi = dev->spi;
    reg_write(spi, REG_FRF_MSB, (uint8_t)(frf >> 16));
    reg_write(spi, REG_FRF_MID, (uint8_t)(frf >>  8));
    reg_write(spi, REG_FRF_LSB, (uint8_t)(frf >>  0));

    dev->frequency = freq_hz;
    return LORA_OK;
}

lora_err_t lora_set_bandwidth(lora_t *dev, uint8_t bw)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (bw > 9) return LORA_ERR_PARAMS;

    const lora_spi_t *spi = dev->spi;
    uint8_t cfg1 = reg_read(spi, REG_MODEM_CONFIG_1);
    cfg1 = (uint8_t)((cfg1 & 0x0F) | (bw << 4));
    reg_write(spi, REG_MODEM_CONFIG_1, cfg1);

    dev->bandwidth = bw;
    return LORA_OK;
}

lora_err_t lora_set_spreading_factor(lora_t *dev, uint8_t sf)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (sf < 6 || sf > 12) return LORA_ERR_PARAMS;

    const lora_spi_t *spi = dev->spi;
    uint8_t cfg2 = reg_read(spi, REG_MODEM_CONFIG_2);
    cfg2 = (uint8_t)((cfg2 & 0x0F) | (sf << 4));
    reg_write(spi, REG_MODEM_CONFIG_2, cfg2);

    dev->spreading_factor = sf;
    return LORA_OK;
}

lora_err_t lora_set_coding_rate(lora_t *dev, uint8_t cr)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (cr < 1 || cr > 4) return LORA_ERR_PARAMS;

    const lora_spi_t *spi = dev->spi;
    uint8_t cfg1 = reg_read(spi, REG_MODEM_CONFIG_1);
    cfg1 = (uint8_t)((cfg1 & 0xF1) | (cr << 1));
    reg_write(spi, REG_MODEM_CONFIG_1, cfg1);

    dev->coding_rate = cr;
    return LORA_OK;
}

lora_err_t lora_set_tx_power(lora_t *dev, int8_t dbm)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (dbm < 2 || dbm > 20) return LORA_ERR_PARAMS;

    const lora_spi_t *spi = dev->spi;

    if (dbm > 17) {
        /* high power mode (+20 dBm) */
        reg_write(spi, REG_PA_DAC, PA_DAC_HIGH_POWER);
        reg_write(spi, REG_PA_CONFIG,
                  (uint8_t)(0x80 | (uint8_t)(dbm - 5)));
    } else {
        reg_write(spi, REG_PA_DAC, PA_DAC_DEFAULT);
        reg_write(spi, REG_PA_CONFIG,
                  (uint8_t)(0x80 | (uint8_t)(dbm - 2)));
    }

    dev->tx_power = dbm;
    return LORA_OK;
}

lora_err_t lora_send(lora_t *dev, const uint8_t *data, uint8_t len)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (!data || len == 0)  return LORA_ERR_PARAMS;

    const lora_spi_t *spi = dev->spi;

    /* standby for FIFO access */
    set_mode(spi, MODE_STANDBY);

    /* set FIFO pointer to TX base */
    reg_write(spi, REG_FIFO_ADDR_PTR, 0x00);

    /* write payload to FIFO */
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, REG_FIFO | SPI_WRITE);
    for (uint8_t i = 0; i < len; i++) {
        spi->xfer(spi->ctx, data[i]);
    }
    spi->cs_high(spi->ctx);

    /* set payload length */
    reg_write(spi, REG_PAYLOAD_LEN, len);

    /* clear IRQ flags */
    reg_write(spi, REG_IRQ_FLAGS, 0xFF);

    /* switch to TX */
    set_mode(spi, MODE_TX);

    /* poll for TxDone */
    for (int i = 0; i < POLL_MAX; i++) {
        spi->delay_ms(spi->ctx, 10);
        uint8_t flags = reg_read(spi, REG_IRQ_FLAGS);
        if (flags & IRQ_TX_DONE) {
            reg_write(spi, REG_IRQ_FLAGS, IRQ_TX_DONE);
            set_mode(spi, MODE_STANDBY);
            return LORA_OK;
        }
    }

    set_mode(spi, MODE_STANDBY);
    return LORA_ERR_TIMEOUT;
}

lora_err_t lora_receive(lora_t *dev, uint8_t *data, uint8_t *len,
                        uint8_t max_len)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (!data || !len)      return LORA_ERR_PARAMS;

    const lora_spi_t *spi = dev->spi;

    /* clear IRQ flags */
    reg_write(spi, REG_IRQ_FLAGS, 0xFF);

    /* set FIFO pointer to RX base */
    reg_write(spi, REG_FIFO_ADDR_PTR, 0x00);

    /* RX single mode */
    set_mode(spi, MODE_RX_SINGLE);

    /* poll for RxDone */
    for (int i = 0; i < POLL_MAX; i++) {
        spi->delay_ms(spi->ctx, 10);
        uint8_t flags = reg_read(spi, REG_IRQ_FLAGS);
        if (flags & IRQ_RX_DONE) {
            /* check CRC error */
            if (flags & IRQ_PAYLOAD_CRC_ERR) {
                reg_write(spi, REG_IRQ_FLAGS, 0xFF);
                set_mode(spi, MODE_STANDBY);
                return LORA_ERR_CRC;
            }

            /* read payload length */
            uint8_t nb = reg_read(spi, REG_RX_NB_BYTES);
            if (nb > max_len) nb = max_len;

            /* set FIFO pointer to current RX address */
            uint8_t rx_addr = reg_read(spi, REG_FIFO_RX_CURRENT);
            reg_write(spi, REG_FIFO_ADDR_PTR, rx_addr);

            /* read FIFO */
            spi->cs_low(spi->ctx);
            spi->xfer(spi->ctx, REG_FIFO & 0x7F);
            for (uint8_t i = 0; i < nb; i++) {
                data[i] = spi->xfer(spi->ctx, 0x00);
            }
            spi->cs_high(spi->ctx);

            *len = nb;
            reg_write(spi, REG_IRQ_FLAGS, 0xFF);
            set_mode(spi, MODE_STANDBY);
            return LORA_OK;
        }
    }

    set_mode(spi, MODE_STANDBY);
    return LORA_ERR_TIMEOUT;
}

bool lora_packet_available(lora_t *dev)
{
    if (!dev || !dev->spi) return false;
    uint8_t flags = reg_read(dev->spi, REG_IRQ_FLAGS);
    return (flags & IRQ_RX_DONE) != 0;
}

int16_t lora_get_rssi(lora_t *dev)
{
    if (!dev || !dev->spi) return 0;
    uint8_t raw = reg_read(dev->spi, REG_PKT_RSSI);
    /* RSSI[dBm] = -157 + raw  (for HF port, freq > 868 MHz) */
    /* RSSI[dBm] = -164 + raw  (for LF port, freq < 868 MHz) */
    int16_t offset = (dev->frequency >= 868000000UL) ? -157 : -164;
    return offset + (int16_t)raw;
}

int8_t lora_get_snr(lora_t *dev)
{
    if (!dev || !dev->spi) return 0;
    uint8_t raw = reg_read(dev->spi, REG_PKT_SNR);
    /* SNR is signed, in units of 0.25 dB. Return integer dB. */
    return (int8_t)raw / 4;
}

lora_err_t lora_sleep(lora_t *dev)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    set_mode(dev->spi, MODE_SLEEP);
    return LORA_OK;
}

lora_err_t lora_standby(lora_t *dev)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    set_mode(dev->spi, MODE_STANDBY);
    return LORA_OK;
}
