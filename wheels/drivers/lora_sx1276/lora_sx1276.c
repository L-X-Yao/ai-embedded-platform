// SPDX-License-Identifier: MIT
/*
 * lora_sx1276 -- SPI LoRa radio transceiver driver for Semtech SX1276/SX1278
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "lora_sx1276.h"
#include "contract.h"

/* ---------- SX1276 register map ---------- */

#define REG_FIFO              0x00
#define REG_OP_MODE           0x01
#define REG_FRF_MSB           0x06
#define REG_FRF_MID           0x07
#define REG_FRF_LSB           0x08
#define REG_PA_CONFIG         0x09
#define REG_OCP               0x0B
#define REG_PA_DAC            0x4D
#define REG_FIFO_ADDR_PTR    0x0D
#define REG_FIFO_TX_BASE     0x0E
#define REG_FIFO_RX_BASE     0x0F
#define REG_FIFO_RX_CURRENT  0x10
#define REG_IRQ_FLAGS         0x12
#define REG_RX_NB_BYTES      0x13
#define REG_HOP_CHANNEL       0x1C
#define REG_PKT_SNR           0x19
#define REG_PKT_RSSI          0x1A
#define REG_MODEM_CONFIG_1   0x1D
#define REG_MODEM_CONFIG_2   0x1E
#define REG_PAYLOAD_LEN      0x22
#define REG_MODEM_CONFIG_3   0x26
#define REG_IMAGE_CAL         0x3B
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

/* RegModemConfig3 bits */
#define CONFIG3_LOW_DATA_RATE 0x08

/* LowDataRateOptimize is mandated above this symbol time */
#define LDRO_SYMBOL_US        16000UL

/* RegHopChannel bits */
#define HOP_CRC_ON_PAYLOAD    0x40

/* RegModemConfig2 bits */
#define CONFIG2_CRC_ON        0x04

/* RegImageCal bits */
#define IMAGE_CAL_START       0x40
#define IMAGE_CAL_RUNNING     0x20

/* Version */
#define SX1276_VERSION        0x12

/* PA_DAC values */
#define PA_DAC_DEFAULT        0x84
#define PA_DAC_HIGH_POWER     0x87

/* RegOcp: the limit must cover 120 mA at +20 dBm */
#define OCP_DEFAULT           0x2B  /* 100 mA */
#define OCP_HIGH_POWER        0x31  /* 140 mA */

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

static lora_err_t set_mode(const lora_spi_t *spi, uint8_t mode)
{
    uint8_t wanted = (uint8_t)(OPMODE_LORA | mode);
    reg_write(spi, REG_OP_MODE, wanted);
    /* Read back to verify mode switch took effect */
    uint8_t actual = reg_read(spi, REG_OP_MODE) & (OPMODE_LORA | 0x07);
    return (actual == wanted) ? LORA_OK : LORA_ERR_SPI;
}

/* Bandwidth in Hz for each RegModemConfig1 code */
static const uint32_t BW_HZ[10] = {
    7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000,
};

static uint32_t symbol_us(const lora_t *dev)
{
    return (uint32_t)(((uint64_t)1000000U << dev->spreading_factor) /
                      BW_HZ[dev->bandwidth]);
}

static bool low_data_rate(const lora_t *dev)
{
    return symbol_us(dev) > LDRO_SYMBOL_US;
}

static void update_low_data_rate(const lora_t *dev)
{
    uint8_t cfg3 = reg_read(dev->spi, REG_MODEM_CONFIG_3);
    cfg3 = (uint8_t)(cfg3 & ~CONFIG3_LOW_DATA_RATE);
    if (low_data_rate(dev)) cfg3 |= CONFIG3_LOW_DATA_RATE;
    reg_write(dev->spi, REG_MODEM_CONFIG_3, cfg3);
}

static lora_err_t calibrate_image(const lora_spi_t *spi)
{
    reg_write(spi, REG_IMAGE_CAL,
              (uint8_t)(reg_read(spi, REG_IMAGE_CAL) | IMAGE_CAL_START));

    for (int i = 0; i < POLL_MAX; i++) {
        if (!(reg_read(spi, REG_IMAGE_CAL) & IMAGE_CAL_RUNNING)) {
            return LORA_OK;
        }
        spi->delay_ms(spi->ctx, 1);
    }
    return LORA_ERR_TIMEOUT;
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

    /* verify chip version */
    uint8_t ver = reg_read(spi, REG_VERSION);
    if (ver != SX1276_VERSION) {
        return LORA_ERR_VERSION;
    }

    /* set frequency */
    lora_err_t err = lora_set_frequency(dev, freq_hz);
    if (err != LORA_OK) return err;

    /* Reset only calibrates at 434 MHz, and calibration runs in FSK mode */
    err = calibrate_image(spi);
    if (err != LORA_OK) return err;

    /* LongRangeMode can only change in Sleep, and reset leaves the chip in
     * standby, so go to Sleep first and then set the bit. */
    reg_write(spi, REG_OP_MODE, MODE_SLEEP);
    err = set_mode(spi, MODE_SLEEP);
    if (err != LORA_OK) return err;
    spi->delay_ms(spi->ctx, 10);

    /* FIFO base addresses: TX=0x00, RX=0x00 (use full 256-byte FIFO) */
    reg_write(spi, REG_FIFO_TX_BASE, 0x00);
    reg_write(spi, REG_FIFO_RX_BASE, 0x00);

    /* AGC auto on */
    reg_write(spi, REG_MODEM_CONFIG_3, 0x04);

    /* In explicit header mode only the transmitter needs this set */
    reg_write(spi, REG_MODEM_CONFIG_2,
              (uint8_t)(reg_read(spi, REG_MODEM_CONFIG_2) | CONFIG2_CRC_ON));

    /* default TX power */
    err = lora_set_tx_power(dev, dev->tx_power);
    if (err != LORA_OK) return err;

    /* standby mode */
    err = set_mode(spi, MODE_STANDBY);
    if (err != LORA_OK) return err;

    return LORA_OK;
}

lora_err_t lora_set_frequency(lora_t *dev, uint32_t freq_hz)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (freq_hz < 137000000UL || freq_hz > 1020000000UL) {
        return LORA_ERR_PARAM;
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
    if (bw > 9) return LORA_ERR_PARAM;

    const lora_spi_t *spi = dev->spi;
    uint8_t cfg1 = reg_read(spi, REG_MODEM_CONFIG_1);
    cfg1 = (uint8_t)((cfg1 & 0x0F) | (bw << 4));
    reg_write(spi, REG_MODEM_CONFIG_1, cfg1);

    dev->bandwidth = bw;
    update_low_data_rate(dev);
    return LORA_OK;
}

lora_err_t lora_set_spreading_factor(lora_t *dev, uint8_t sf)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (sf < 7 || sf > 12) return LORA_ERR_PARAM;

    const lora_spi_t *spi = dev->spi;
    uint8_t cfg2 = reg_read(spi, REG_MODEM_CONFIG_2);
    cfg2 = (uint8_t)((cfg2 & 0x0F) | (sf << 4));
    reg_write(spi, REG_MODEM_CONFIG_2, cfg2);

    dev->spreading_factor = sf;
    update_low_data_rate(dev);
    return LORA_OK;
}

lora_err_t lora_set_coding_rate(lora_t *dev, uint8_t cr)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (cr < 1 || cr > 4) return LORA_ERR_PARAM;

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
    if (dbm < 2 || dbm > 20) return LORA_ERR_PARAM;

    const lora_spi_t *spi = dev->spi;

    if (dbm > 17) {
        /* high power mode (+20 dBm) */
        reg_write(spi, REG_PA_DAC, PA_DAC_HIGH_POWER);
        reg_write(spi, REG_OCP, OCP_HIGH_POWER);
        reg_write(spi, REG_PA_CONFIG,
                  (uint8_t)(0x80 | (uint8_t)(dbm - 5)));
    } else {
        reg_write(spi, REG_PA_DAC, PA_DAC_DEFAULT);
        reg_write(spi, REG_OCP, OCP_DEFAULT);
        reg_write(spi, REG_PA_CONFIG,
                  (uint8_t)(0x80 | (uint8_t)(dbm - 2)));
    }

    dev->tx_power = dbm;
    return LORA_OK;
}

lora_err_t lora_send(lora_t *dev, const uint8_t *data, uint8_t len)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (!data || len == 0)  return LORA_ERR_PARAM;

    const lora_spi_t *spi = dev->spi;

    /* standby for FIFO access */
    lora_err_t merr = set_mode(spi, MODE_STANDBY);
    if (merr != LORA_OK) return merr;

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
    merr = set_mode(spi, MODE_TX);
    if (merr != LORA_OK) return merr;

    /* poll for TxDone */
    for (int i = 0; i < POLL_MAX; i++) {
        spi->delay_ms(spi->ctx, 10);
        uint8_t flags = reg_read(spi, REG_IRQ_FLAGS);
        if (flags & IRQ_TX_DONE) {
            reg_write(spi, REG_IRQ_FLAGS, IRQ_TX_DONE);
            (void)set_mode(spi, MODE_STANDBY);
            return LORA_OK;
        }
    }

    (void)set_mode(spi, MODE_STANDBY);
    return LORA_ERR_TIMEOUT;
}

lora_err_t lora_receive(lora_t *dev, uint8_t *data, uint8_t *len,
                        uint8_t max_len)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    if (!data || !len)      return LORA_ERR_PARAM;

    const lora_spi_t *spi = dev->spi;

    /* clear IRQ flags */
    reg_write(spi, REG_IRQ_FLAGS, 0xFF);

    /* set FIFO pointer to RX base */
    reg_write(spi, REG_FIFO_ADDR_PTR, 0x00);

    /* RX single mode */
    lora_err_t merr = set_mode(spi, MODE_RX_SINGLE);
    if (merr != LORA_OK) return merr;

    /* poll for RxDone */
    for (int i = 0; i < POLL_MAX; i++) {
        spi->delay_ms(spi->ctx, 10);
        uint8_t flags = reg_read(spi, REG_IRQ_FLAGS);
        if (flags & IRQ_RX_DONE) {
            /* PayloadCrcError means nothing unless the packet carried a CRC */
            if ((flags & IRQ_PAYLOAD_CRC_ERR) ||
                !(reg_read(spi, REG_HOP_CHANNEL) & HOP_CRC_ON_PAYLOAD)) {
                reg_write(spi, REG_IRQ_FLAGS, 0xFF);
                (void)set_mode(spi, MODE_STANDBY);
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
            for (uint8_t j = 0; j < nb; j++) {
                data[j] = spi->xfer(spi->ctx, 0x00);
            }
            spi->cs_high(spi->ctx);

            *len = nb;
            reg_write(spi, REG_IRQ_FLAGS, 0xFF);
            (void)set_mode(spi, MODE_STANDBY);
            return LORA_OK;
        }
    }

    (void)set_mode(spi, MODE_STANDBY);
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
    /*
     * SNR register is signed two's-complement in units of 0.25 dB.
     * Cast to int8_t first to get the signed value, then divide.
     * Use explicit signed variable to avoid C integer promotion pitfalls.
     * Returns integer dB (truncated toward zero; e.g. -3 raw → 0 dB).
     */
    int8_t signed_raw = (int8_t)raw;
    return (int8_t)(signed_raw / 4);
}

lora_err_t lora_sleep(lora_t *dev)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    return set_mode(dev->spi, MODE_SLEEP);
}

lora_err_t lora_standby(lora_t *dev)
{
    if (!dev || !dev->spi) return LORA_ERR_SPI;
    return set_mode(dev->spi, MODE_STANDBY);
}
