// SPDX-License-Identifier: MIT
/*
 * cc1101 — TI CC1101 Sub-GHz transceiver driver (SPI)
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "cc1101.h"
#include "contract.h"
#include <string.h>

/* ---------- CC1101 register addresses ---------- */

#define REG_IOCFG2          0x00U
#define REG_IOCFG1          0x01U
#define REG_IOCFG0          0x02U
#define REG_PKTCTRL1        0x07U
#define REG_PKTCTRL0        0x08U
#define REG_FREQ2           0x0DU
#define REG_FREQ1           0x0EU
#define REG_FREQ0           0x0FU
#define REG_MDMCFG4         0x10U
#define REG_MDMCFG3         0x11U
#define REG_MDMCFG2         0x12U
#define REG_MCSM0           0x18U
#define REG_PKTLEN          0x06U
#define REG_PATABLE         0x3EU
#define REG_TX_FIFO         0x3FU
#define REG_RX_FIFO         0x3FU

/* Status registers (read with 0xC0 | addr) */
#define SREG_PARTNUM        0x30U
#define SREG_VERSION        0x31U
#define SREG_RSSI           0x34U
#define SREG_MARCSTATE      0x35U
#define SREG_RXBYTES        0x3BU
#define SREG_TXBYTES        0x3AU

/* Command strobes */
#define STROBE_SRES         0x30U
#define STROBE_SRX          0x34U
#define STROBE_STX          0x35U
#define STROBE_SIDLE        0x36U
#define STROBE_SFRX         0x3AU
#define STROBE_SFTX         0x3BU
#define STROBE_SNOP         0x3DU

/* SPI header bits */
#define SPI_READ            0x80U
#define SPI_BURST           0x40U
#define SPI_STATUS_READ     0xC0U  /* SPI_READ | SPI_BURST for status regs */

/* Expected ID value. VERSION is not fixed, a bus with no chip reads 0x00 or 0xFF */
#define CC1101_PARTNUM      0x00U
#define VERSION_NO_CHIP_LOW  0x00U
#define VERSION_NO_CHIP_HIGH 0xFFU

/* MCSM0: FS_AUTOCAL = 01 (calibrate going from IDLE to RX or TX), the rest at reset */
#define MCSM0_AUTOCAL       0x14U

/* IOCFG: high impedance instead of the default clock output on GDO0 */
#define IOCFG_HIGH_Z        0x2EU

/* PKTCTRL1: append the two status bytes to each received packet */
#define PKTCTRL1_APPEND     0x04U

/* Packet limits: the 64-byte FIFO also holds length and status bytes */
#define PKT_MAX_LEN         61U
#define RX_STATUS_BYTES     2U
#define RX_STATUS_CRC_OK    0x80U

/* PA settings the datasheet forbids */
#define PA_FORBIDDEN_LOW    0x61U
#define PA_FORBIDDEN_HIGH   0x6FU

/* Crystal oscillator frequency */
#define FXOSC               26000000UL

/* MARCSTATE values */
#define MARCSTATE_IDLE      0x01U
#define MARCSTATE_RX        0x0DU
#define MARCSTATE_TX        0x13U

/* Polling limits */
#define POLL_MAX            200U

/* ---------- internal helpers ---------- */

static uint8_t spi_strobe(const cc1101_hal_t *hal, uint8_t strobe)
{
    uint8_t status;
    hal->cs_select(hal->ctx);
    status = hal->xfer(hal->ctx, strobe);
    hal->cs_deselect(hal->ctx);
    return status;
}

static void reg_write(const cc1101_hal_t *hal, uint8_t addr, uint8_t val)
{
    hal->cs_select(hal->ctx);
    (void)hal->xfer(hal->ctx, addr & 0x3FU);
    (void)hal->xfer(hal->ctx, val);
    hal->cs_deselect(hal->ctx);
}

static uint8_t reg_read(const cc1101_hal_t *hal, uint8_t addr)
{
    uint8_t val;
    hal->cs_select(hal->ctx);
    (void)hal->xfer(hal->ctx, (uint8_t)((addr & 0x3FU) | SPI_READ));
    val = hal->xfer(hal->ctx, 0x00);
    hal->cs_deselect(hal->ctx);
    return val;
}

static uint8_t status_reg_read(const cc1101_hal_t *hal, uint8_t addr)
{
    uint8_t val;
    hal->cs_select(hal->ctx);
    (void)hal->xfer(hal->ctx, (addr & 0x3FU) | SPI_STATUS_READ);
    val = hal->xfer(hal->ctx, 0x00);
    hal->cs_deselect(hal->ctx);
    return val;
}

static void fifo_write_burst(const cc1101_hal_t *hal,
                             const uint8_t *data, uint8_t len)
{
    uint8_t i;
    hal->cs_select(hal->ctx);
    (void)hal->xfer(hal->ctx, REG_TX_FIFO | SPI_BURST);
    for (i = 0; i < len; i++) {
        (void)hal->xfer(hal->ctx, data[i]);
    }
    hal->cs_deselect(hal->ctx);
}

static void fifo_read_burst(const cc1101_hal_t *hal,
                            uint8_t *data, uint8_t len)
{
    uint8_t i;
    hal->cs_select(hal->ctx);
    (void)hal->xfer(hal->ctx, REG_RX_FIFO | SPI_STATUS_READ);
    for (i = 0; i < len; i++) {
        data[i] = hal->xfer(hal->ctx, 0x00);
    }
    hal->cs_deselect(hal->ctx);
}

/* One read can be corrupt while the radio updates the register (SWRS061I p.32),
 * so a value counts once two reads in a row agree. */
static uint8_t get_marcstate(const cc1101_hal_t *hal)
{
    uint8_t state = status_reg_read(hal, SREG_MARCSTATE);

    for (uint8_t i = 0; i < 4U; i++) {
        uint8_t again = status_reg_read(hal, SREG_MARCSTATE);
        if (again == state) {
            break;
        }
        state = again;
    }
    return state & 0x1FU;
}

static bool frequency_supported(uint32_t freq_hz)
{
    return (freq_hz >= 300000000UL && freq_hz <= 348000000UL) ||
           (freq_hz >= 387000000UL && freq_hz <= 464000000UL) ||
           (freq_hz >= 779000000UL && freq_hz <= 928000000UL);
}

/* ---------- public API ---------- */

/* The steps of init that talk to the chip */
static cc1101_err_t bring_up(const cc1101_hal_t *hal)
{
    uint8_t partnum, version;

    /* Reset */
    (void)spi_strobe(hal, STROBE_SRES);
    hal->delay_ms(hal->ctx, 10);

    /* Check for SPI bus error after reset strobe */
    if (hal->get_spi_error && hal->get_spi_error(hal->ctx)) {
        return CC1101_ERR_SPI;
    }

    /* Verify chip ID */
    partnum = status_reg_read(hal, SREG_PARTNUM);
    version = status_reg_read(hal, SREG_VERSION);

    if (partnum != CC1101_PARTNUM || version == VERSION_NO_CHIP_LOW ||
        version == VERSION_NO_CHIP_HIGH) {
        return CC1101_ERR_ID;
    }

    /* Configure: variable packet length, CRC enabled */
    reg_write(hal, REG_PKTCTRL0, 0x45U); /* variable len + CRC + whitening */
    reg_write(hal, REG_PKTCTRL1, PKTCTRL1_APPEND);
    reg_write(hal, REG_PKTLEN, PKT_MAX_LEN);

    /* The synthesizer has to be calibrated before use and for each new frequency */
    reg_write(hal, REG_MCSM0, MCSM0_AUTOCAL);
    reg_write(hal, REG_IOCFG0, IOCFG_HIGH_Z);

    /* Enter idle */
    (void)spi_strobe(hal, STROBE_SIDLE);

    return CC1101_OK;
}

cc1101_err_t cc1101_init(cc1101_t *dev, const cc1101_hal_t *hal)
{
    if (!dev) return CC1101_ERR_PARAM;

    /* dev->hal stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!hal) return CC1101_ERR_PARAM;
    if (!hal->cs_select || !hal->cs_deselect || !hal->xfer || !hal->delay_ms) {
        return CC1101_ERR_PARAM;
    }

    dev->hal = hal;

    cc1101_err_t err = bring_up(hal);
    if (err != CC1101_OK) {
        dev->hal = NULL;
    }
    return err;
}

cc1101_err_t cc1101_set_frequency(cc1101_t *dev, uint32_t freq_hz)
{
    uint32_t freq_word;

    if (!dev || !dev->hal) return CC1101_ERR_PARAM;
    if (!frequency_supported(freq_hz)) {
        return CC1101_ERR_PARAM;
    }

    /* FREQ = freq_hz * 2^16 / FXOSC */
    freq_word = (uint32_t)(((uint64_t)freq_hz << 16) / FXOSC);

    reg_write(dev->hal, REG_FREQ2, (uint8_t)(freq_word >> 16));
    reg_write(dev->hal, REG_FREQ1, (uint8_t)(freq_word >>  8));
    reg_write(dev->hal, REG_FREQ0, (uint8_t)(freq_word >>  0));

    dev->frequency = freq_hz;
    return CC1101_OK;
}

cc1101_err_t cc1101_set_power(cc1101_t *dev, uint8_t pa_value)
{
    if (!dev || !dev->hal) return CC1101_ERR_PARAM;
    if (pa_value >= PA_FORBIDDEN_LOW && pa_value <= PA_FORBIDDEN_HIGH) {
        return CC1101_ERR_PARAM;
    }

    reg_write(dev->hal, REG_PATABLE, pa_value);
    dev->tx_power = (int8_t)pa_value;
    return CC1101_OK;
}

cc1101_err_t cc1101_transmit(cc1101_t *dev, const uint8_t *data, uint8_t len)
{
    uint32_t i;
    uint8_t marcstate;

    if (!dev || !dev->hal) return CC1101_ERR_PARAM;
    if (!data || len == 0 || len > 61) return CC1101_ERR_PARAM;

    /* Ensure idle before TX */
    (void)spi_strobe(dev->hal, STROBE_SIDLE);
    (void)spi_strobe(dev->hal, STROBE_SFTX);

    /* Write length byte + payload to TX FIFO */
    {
        uint8_t pkt[64];
        pkt[0] = len;
        uint8_t j;
        for (j = 0; j < len; j++) {
            pkt[j + 1] = data[j];
        }
        fifo_write_burst(dev->hal, pkt, (uint8_t)(len + 1));
    }

    /* Strobe TX */
    (void)spi_strobe(dev->hal, STROBE_STX);

    /* Poll for TX completion (MARCSTATE returns to IDLE) */
    for (i = 0; i < POLL_MAX; i++) {
        dev->hal->delay_ms(dev->hal->ctx, 5);
        marcstate = get_marcstate(dev->hal);
        if (marcstate == MARCSTATE_IDLE) {
            return CC1101_OK;
        }
    }

    (void)spi_strobe(dev->hal, STROBE_SIDLE);
    return CC1101_ERR_TIMEOUT;
}

cc1101_err_t cc1101_receive(cc1101_t *dev, uint8_t *buf, uint8_t cap,
                            uint8_t *out_len)
{
    uint32_t i;
    uint8_t marcstate;
    uint8_t rxbytes;
    uint8_t pkt_len;

    if (!dev || !dev->hal) return CC1101_ERR_PARAM;
    if (!buf || !out_len || cap == 0) return CC1101_ERR_PARAM;

    /* Flush RX FIFO and enter RX */
    (void)spi_strobe(dev->hal, STROBE_SIDLE);
    (void)spi_strobe(dev->hal, STROBE_SFRX);
    (void)spi_strobe(dev->hal, STROBE_SRX);

    /* Poll for RX completion */
    for (i = 0; i < POLL_MAX; i++) {
        dev->hal->delay_ms(dev->hal->ctx, 5);
        marcstate = get_marcstate(dev->hal);
        if (marcstate == MARCSTATE_IDLE) {
            /* Packet received — RX auto-returns to idle */
            rxbytes = status_reg_read(dev->hal, SREG_RXBYTES) & 0x7FU;
            if (rxbytes == 0) {
                return CC1101_ERR_TIMEOUT;
            }

            /* The FIFO holds the length byte, the payload and two status bytes */
            pkt_len = reg_read(dev->hal, REG_RX_FIFO | SPI_STATUS_READ);
            if (pkt_len == 0 || pkt_len > PKT_MAX_LEN ||
                (uint16_t)pkt_len + 1U + RX_STATUS_BYTES > rxbytes) {
                (void)spi_strobe(dev->hal, STROBE_SFRX);
                return CC1101_ERR_CRC;
            }

            uint8_t pkt[PKT_MAX_LEN + RX_STATUS_BYTES];
            fifo_read_burst(dev->hal, pkt, (uint8_t)(pkt_len + RX_STATUS_BYTES));

            if (!(pkt[pkt_len + 1U] & RX_STATUS_CRC_OK)) {
                return CC1101_ERR_CRC;
            }

            uint8_t read_len = (pkt_len < cap) ? pkt_len : cap;
            memcpy(buf, pkt, read_len);
            *out_len = read_len;

            return CC1101_OK;
        }
    }

    (void)spi_strobe(dev->hal, STROBE_SIDLE);
    return CC1101_ERR_TIMEOUT;
}

int16_t cc1101_get_rssi(cc1101_t *dev)
{
    uint8_t raw;
    int16_t rssi;

    if (!dev || !dev->hal) return 0;

    raw = status_reg_read(dev->hal, SREG_RSSI);

    /* RSSI conversion per CC1101 datasheet:
     * If raw >= 128: rssi_dbm = (raw - 256) / 2 - 74
     * If raw <  128: rssi_dbm = raw / 2 - 74 */
    if (raw >= 128) {
        rssi = (int16_t)((int16_t)(raw - 256) / 2 - 74);
    } else {
        rssi = (int16_t)((int16_t)raw / 2 - 74);
    }
    return rssi;
}

cc1101_err_t cc1101_idle(cc1101_t *dev)
{
    if (!dev || !dev->hal) return CC1101_ERR_PARAM;

    (void)spi_strobe(dev->hal, STROBE_SIDLE);
    return CC1101_OK;
}
