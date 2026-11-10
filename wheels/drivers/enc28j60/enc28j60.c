// SPDX-License-Identifier: MIT
/*
 * enc28j60 -- SPI Ethernet controller driver for Microchip ENC28J60
 *
 * Layer:   L1
 * Status:  candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 */

#include "enc28j60.h"
#include "contract.h"
#include <string.h>

/* ---------- SPI opcodes ---------- */

#define OP_RCR  0x00U   /* Read Control Register  */
#define OP_RBM  0x3AU   /* Read Buffer Memory     */
#define OP_WCR  0x40U   /* Write Control Register */
#define OP_WBM  0x7AU   /* Write Buffer Memory    */
#define OP_BFS  0x80U   /* Bit Field Set           */
#define OP_BFC  0xA0U   /* Bit Field Clear         */
#define OP_SRC  0xFFU   /* System Reset Command    */

/* ---------- register addresses (5-bit) ---------- */

/* Bank 0 */
#define ERDPTL    0x00U
#define ERDPTH    0x01U
#define EWRPTL    0x02U
#define EWRPTH    0x03U
#define ETXSTL    0x04U
#define ETXSTH    0x05U
#define ETXNDL    0x06U
#define ETXNDH    0x07U
#define ERXSTL    0x08U
#define ERXSTH    0x09U
#define ERXNDL    0x0AU
#define ERXNDH    0x0BU
#define ERXRDPTL  0x0CU
#define ERXRDPTH  0x0DU

/* Bank 1 */
#define ERXFCON   0x18U
#define EPKTCNT   0x19U

/* Bank 2 */
#define MACON1    0x00U
#define MACON3    0x02U
#define MACON4    0x03U
#define MAMXFLL   0x0AU
#define MAMXFLH   0x0BU
#define MABBIPG   0x04U
#define MAIPGL    0x06U
#define MAIPGH    0x07U
#define MIREGADR  0x14U
#define MIWRL     0x16U
#define MIWRH     0x17U

/* Bank 3 */
#define MAADR5    0x00U
#define MAADR6    0x01U
#define MAADR3    0x02U
#define MAADR4    0x03U
#define MAADR1    0x04U
#define MAADR2    0x05U
#define MISTAT    0x0AU
#define EREVID    0x12U

/* Common (all banks) */
#define EIE       0x1BU
#define EIR       0x1CU
#define ESTAT     0x1DU
#define ECON2     0x1EU
#define ECON1     0x1FU

/* PHY registers (reached through MIREGADR) */
#define PHCON1    0x00U
#define PHCON2    0x10U

/* ---------- bit masks ---------- */

#define ECON1_BSEL0   0x01U
#define ECON1_BSEL1   0x02U
#define ECON1_RXEN    0x04U
#define ECON1_TXRTS   0x08U
#define ECON1_TXRST   0x80U

#define ECON2_PKTDEC  0x40U

#define EIR_TXERIF    0x02U
#define EIR_TXIF      0x08U

#define ESTAT_CLKRDY  0x01U
#define ESTAT_TXABRT  0x02U
#define ESTAT_RXBUSY  0x04U
#define ESTAT_LATECOL 0x10U

#define MACON1_MARXEN 0x01U
#define MACON3_PADCFG0 0x20U
#define MACON3_TXCRCEN 0x10U
#define MACON3_FRMLNEN 0x02U
#define MACON3_FULDPX  0x01U
#define MACON4_DEFER   0x40U

#define MISTAT_BUSY   0x01U

#define PHCON2_HDLDIS 0x0100U

#define ERXFCON_UCEN  0x80U
#define ERXFCON_CRCEN 0x20U
#define ERXFCON_BCEN  0x01U

/* ---------- buffer layout ---------- */

#define RXSTART  0x0000U
#define RXEND    0x19FFU
#define TXSTART  0x1A00U
#define TXEND    0x1FFFU

/* MACON3.TXCRCEN appends this much to each frame, and MAMXFL counts it */
#define CRC_LEN  4U

/* ---------- timing ---------- */

/* Errata DS80349C issue 2: wait at least 1 ms after an SPI Reset */
#ifndef ENC28J60_RESET_DELAY_MS
#define ENC28J60_RESET_DELAY_MS  2U
#endif

#ifndef ENC28J60_TX_TIMEOUT
#define ENC28J60_TX_TIMEOUT  50000U
#endif

/* Reads of a status bit, 1 ms apart, before giving up */
#ifndef ENC28J60_READY_POLLS
#define ENC28J60_READY_POLLS  10U
#endif

/* ---------- helpers: low-level SPI ---------- */

static void spi_write_op(const enc28j60_spi_t *s, uint8_t op,
                          uint8_t addr, uint8_t data)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, (uint8_t)(op | (addr & 0x1FU)));
    s->xfer(s->ctx, data);
    s->cs_high(s->ctx);
}

static uint8_t spi_read_op(const enc28j60_spi_t *s, uint8_t op,
                            uint8_t addr, bool mac_mii)
{
    s->cs_low(s->ctx);
    s->xfer(s->ctx, (uint8_t)(op | (addr & 0x1FU)));
    if (mac_mii) {
        (void)s->xfer(s->ctx, 0xFFU);   /* dummy byte for MAC/MII regs */
    }
    uint8_t val = s->xfer(s->ctx, 0xFFU);
    s->cs_high(s->ctx);
    return val;
}

/* ---------- helpers: bank select ---------- */

static void bank_select(enc28j60_t *dev, uint8_t bank)
{
    if (dev->bank == bank) return;
    /* Clear bank bits, then set new ones */
    spi_write_op(dev->spi, OP_BFC, ECON1, ECON1_BSEL0 | ECON1_BSEL1);
    spi_write_op(dev->spi, OP_BFS, ECON1, bank & 0x03U);
    dev->bank = bank;
}

/* ---------- helpers: register read/write (banked) ---------- */

static uint8_t read_reg(enc28j60_t *dev, uint8_t bank, uint8_t addr,
                        bool mac_mii)
{
    bank_select(dev, bank);
    return spi_read_op(dev->spi, OP_RCR, addr, mac_mii);
}

static void write_reg(enc28j60_t *dev, uint8_t bank, uint8_t addr,
                      uint8_t val)
{
    bank_select(dev, bank);
    spi_write_op(dev->spi, OP_WCR, addr, val);
}

/* ---------- helpers: PHY register write ---------- */

/* DS39662C section 3.3.2: writing MIWRH starts the transfer, which takes
 * 10.24 us. PHY registers cannot be written bit by bit. */
static enc28j60_err_t phy_write(enc28j60_t *dev, uint8_t reg, uint16_t val)
{
    write_reg(dev, 2, MIREGADR, reg);
    write_reg(dev, 2, MIWRL, (uint8_t)(val & 0xFFU));
    write_reg(dev, 2, MIWRH, (uint8_t)(val >> 8));

    for (uint32_t i = 0; i < ENC28J60_READY_POLLS; i++) {
        if (!(read_reg(dev, 3, MISTAT, true) & MISTAT_BUSY)) {
            return ENC28J60_OK;
        }
        dev->spi->delay_ms(dev->spi->ctx, 1U);
    }
    return ENC28J60_ERR_TIMEOUT;
}

/* ---------- helpers: buffer read/write ---------- */

static void write_buffer(enc28j60_t *dev, const uint8_t *data, uint16_t len)
{
    dev->spi->cs_low(dev->spi->ctx);
    dev->spi->xfer(dev->spi->ctx, OP_WBM);
    for (uint16_t i = 0; i < len; i++) {
        dev->spi->xfer(dev->spi->ctx, data[i]);
    }
    dev->spi->cs_high(dev->spi->ctx);
}

static void read_buffer(enc28j60_t *dev, uint8_t *data, uint16_t len)
{
    dev->spi->cs_low(dev->spi->ctx);
    dev->spi->xfer(dev->spi->ctx, OP_RBM);
    for (uint16_t i = 0; i < len; i++) {
        data[i] = dev->spi->xfer(dev->spi->ctx, 0xFFU);
    }
    dev->spi->cs_high(dev->spi->ctx);
}

/* ---------- helpers: bit field ops on common regs ---------- */

static void bit_field_set(enc28j60_t *dev, uint8_t addr, uint8_t mask)
{
    spi_write_op(dev->spi, OP_BFS, addr, mask);
}

static void bit_field_clear(enc28j60_t *dev, uint8_t addr, uint8_t mask)
{
    spi_write_op(dev->spi, OP_BFC, addr, mask);
}

/* ---------- soft reset ---------- */

static void soft_reset(enc28j60_t *dev)
{
    dev->spi->cs_low(dev->spi->ctx);
    dev->spi->xfer(dev->spi->ctx, OP_SRC);
    dev->spi->cs_high(dev->spi->ctx);
    dev->bank = 0;
    /* Errata DS80349C issue 2: SRC stops the PHY clock without clearing
     * CLKRDY, so only this wait covers it. */
    dev->spi->delay_ms(dev->spi->ctx, ENC28J60_RESET_DELAY_MS);
}

/* ---------- wait for ESTAT bits ---------- */

static enc28j60_err_t wait_estat(enc28j60_t *dev, uint8_t mask, uint8_t want)
{
    for (uint32_t i = 0; i < ENC28J60_READY_POLLS; i++) {
        if ((spi_read_op(dev->spi, OP_RCR, ESTAT, false) & mask) == want) {
            return ENC28J60_OK;
        }
        dev->spi->delay_ms(dev->spi->ctx, 1U);
    }
    return ENC28J60_ERR_TIMEOUT;
}

/* ---------- internal: write MAC address to registers ---------- */

static void write_mac_regs(enc28j60_t *dev)
{
    write_reg(dev, 3, MAADR1, dev->mac_addr[0]);
    write_reg(dev, 3, MAADR2, dev->mac_addr[1]);
    write_reg(dev, 3, MAADR3, dev->mac_addr[2]);
    write_reg(dev, 3, MAADR4, dev->mac_addr[3]);
    write_reg(dev, 3, MAADR5, dev->mac_addr[4]);
    write_reg(dev, 3, MAADR6, dev->mac_addr[5]);
}

/* ---------- API ---------- */

/* The steps of init that talk to the chip */
static enc28j60_err_t bring_up(enc28j60_t *dev)
{
    /* Soft reset */
    soft_reset(dev);

    /* DS39662C p.7: after a Power-on Reset, poll CLKRDY before any MAC, MII
     * or PHY access and before setting RXEN or TXRTS */
    enc28j60_err_t err = wait_estat(dev, ESTAT_CLKRDY, ESTAT_CLKRDY);
    if (err != ENC28J60_OK) return err;

    /* RX buffer: 0x0000 - 0x19FF */
    write_reg(dev, 0, ERXSTL, (uint8_t)(RXSTART & 0xFFU));
    write_reg(dev, 0, ERXSTH, (uint8_t)(RXSTART >> 8));
    write_reg(dev, 0, ERXNDL, (uint8_t)(RXEND & 0xFFU));
    write_reg(dev, 0, ERXNDH, (uint8_t)(RXEND >> 8));

    /* Errata DS80349C issue 14: an even ERXRDPT may corrupt the receive
     * buffer, so this is ERXND and not the ERXST of DS39662C p.35 */
    write_reg(dev, 0, ERXRDPTL, (uint8_t)(RXEND & 0xFFU));
    write_reg(dev, 0, ERXRDPTH, (uint8_t)(RXEND >> 8));

    /* RX read pointer */
    write_reg(dev, 0, ERDPTL, (uint8_t)(RXSTART & 0xFFU));
    write_reg(dev, 0, ERDPTH, (uint8_t)(RXSTART >> 8));

    /* RX filters: unicast + broadcast + CRC check */
    write_reg(dev, 1, ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN);

    /* MAC init */
    write_reg(dev, 2, MACON1, MACON1_MARXEN);
    write_reg(dev, 2, MACON3,
              MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN);
    write_reg(dev, 2, MACON4, MACON4_DEFER);

    /* Max frame length */
    write_reg(dev, 2, MAMXFLL, (uint8_t)(ENC28J60_MAX_FRAME & 0xFFU));
    write_reg(dev, 2, MAMXFLH, (uint8_t)(ENC28J60_MAX_FRAME >> 8));

    /* Back-to-back inter-packet gap (half-duplex recommended = 0x12) */
    write_reg(dev, 2, MABBIPG, 0x12U);

    /* Non-back-to-back inter-packet gap */
    write_reg(dev, 2, MAIPGL, 0x12U);
    write_reg(dev, 2, MAIPGH, 0x0CU);

    /* Set MAC address */
    write_mac_regs(dev);

    /* PHY half duplex, to match MACON3.FULDPX = 0: PDPXMD resets to what the
     * LEDB wiring says, and to an unknown value with no LED. All the other
     * PHCON1 bits reset to 0 as well. */
    err = phy_write(dev, PHCON1, 0x0000U);
    if (err != ENC28J60_OK) return err;

    /* Half duplex loops transmitted frames back by default, and the filters
     * above would pass the broadcast ones up to enc28j60_receive */
    err = phy_write(dev, PHCON2, PHCON2_HDLDIS);
    if (err != ENC28J60_OK) return err;

    /* Enable reception */
    bit_field_set(dev, ECON1, ECON1_RXEN);

    return ENC28J60_OK;
}

enc28j60_err_t enc28j60_init(enc28j60_t *dev, const enc28j60_spi_t *spi,
                              const uint8_t *mac_addr)
{
    if (!dev)
        return ENC28J60_ERR_PARAM;

    /* dev->spi stays NULL unless init succeeds: the other calls refuse a
     * device without it */
    memset(dev, 0, sizeof(*dev));

    if (!spi || !spi->cs_low || !spi->cs_high || !spi->xfer ||
        !spi->delay_ms)
        return ENC28J60_ERR_PARAM;
    if (!mac_addr)
        return ENC28J60_ERR_PARAM;

    dev->spi  = spi;
    dev->bank = 0;
    dev->next_packet_ptr = RXSTART;
    memcpy(dev->mac_addr, mac_addr, 6);

    enc28j60_err_t err = bring_up(dev);
    if (err != ENC28J60_OK)
        dev->spi = NULL;
    return err;
}

enc28j60_err_t enc28j60_send(enc28j60_t *dev, const uint8_t *frame,
                              uint16_t len)
{
    if (!dev || !dev->spi) return ENC28J60_ERR_PARAM;
    if (!frame)            return ENC28J60_ERR_PARAM;
    if (len == 0)          return ENC28J60_ERR_SIZE;
    if (len > ENC28J60_MAX_FRAME - CRC_LEN) return ENC28J60_ERR_SIZE;

    /* Set TX buffer pointers */
    write_reg(dev, 0, EWRPTL, (uint8_t)(TXSTART & 0xFFU));
    write_reg(dev, 0, EWRPTH, (uint8_t)(TXSTART >> 8));
    write_reg(dev, 0, ETXSTL, (uint8_t)(TXSTART & 0xFFU));
    write_reg(dev, 0, ETXSTH, (uint8_t)(TXSTART >> 8));

    /* Write per-packet control byte (0x00 = use MACON3 settings) + frame */
    {
        uint8_t ctrl = 0x00U;
        write_buffer(dev, &ctrl, 1);
    }
    write_buffer(dev, frame, len);

    /* Set TX end pointer */
    uint16_t txend = (uint16_t)(TXSTART + 1U + len - 1U);
    write_reg(dev, 0, ETXNDL, (uint8_t)(txend & 0xFFU));
    write_reg(dev, 0, ETXNDH, (uint8_t)(txend >> 8));

    /* Errata DS80349C issue 12: a half duplex abort may stall the transmit
     * logic, and the next TXRTS would then never clear. Releasing TXRST may
     * set TXERIF, so Example 1 clears the flags after it. */
    bit_field_set(dev, ECON1, ECON1_TXRST);
    bit_field_clear(dev, ECON1, ECON1_TXRST);
    bit_field_clear(dev, EIR, EIR_TXERIF | EIR_TXIF);

    /* Only the result of this frame counts: the host has to clear these */
    bit_field_clear(dev, ESTAT, ESTAT_TXABRT | ESTAT_LATECOL);

    /* Start transmission */
    bit_field_set(dev, ECON1, ECON1_TXRTS);

    /* Wait for TX complete (TXRTS clears when done, aborted or not) */
    uint32_t timeout = ENC28J60_TX_TIMEOUT;
    while (timeout--) {
        uint8_t econ1 = spi_read_op(dev->spi, OP_RCR, ECON1, false);
        if (!(econ1 & ECON1_TXRTS)) {
            uint8_t estat = spi_read_op(dev->spi, OP_RCR, ESTAT, false);
            return (estat & ESTAT_TXABRT) ? ENC28J60_ERR_TX_ABORT
                                          : ENC28J60_OK;
        }
    }

    /* Cancel it, or the next send would overwrite a pending transmission */
    bit_field_clear(dev, ECON1, ECON1_TXRTS);
    return ENC28J60_ERR_TIMEOUT;
}

/* Give a read packet's room back and move on to the next one */
static void free_packet(enc28j60_t *dev, uint16_t next_pkt)
{
    dev->next_packet_ptr = next_pkt;

    /* Free RX buffer space (errata: ERXRDPT must be odd) */
    if (next_pkt == RXSTART) {
        write_reg(dev, 0, ERXRDPTL, (uint8_t)(RXEND & 0xFFU));
        write_reg(dev, 0, ERXRDPTH, (uint8_t)(RXEND >> 8));
    } else {
        write_reg(dev, 0, ERXRDPTL, (uint8_t)((next_pkt - 1U) & 0xFFU));
        write_reg(dev, 0, ERXRDPTH, (uint8_t)((next_pkt - 1U) >> 8));
    }

    /* Decrement packet counter */
    bit_field_set(dev, ECON2, ECON2_PKTDEC);
}

enc28j60_err_t enc28j60_receive(enc28j60_t *dev, uint8_t *frame,
                                 uint16_t buf_size, uint16_t *frame_len)
{
    if (!dev || !dev->spi) return ENC28J60_ERR_PARAM;
    if (!frame || !frame_len) return ENC28J60_ERR_PARAM;

    /* Check packet count.
     * EPKTCNT (bank1:0x19) is an ETH register per datasheet table 3-1,
     * not MAC/MII -- no dummy byte required. */
    uint8_t pktcnt = read_reg(dev, 1, EPKTCNT, false);
    if (pktcnt == 0) return ENC28J60_ERR_NO_PACKET;

    /* Set read pointer to next packet */
    write_reg(dev, 0, ERDPTL, (uint8_t)(dev->next_packet_ptr & 0xFFU));
    write_reg(dev, 0, ERDPTH, (uint8_t)(dev->next_packet_ptr >> 8));

    /* Read next packet pointer (2 bytes, little-endian) */
    uint8_t hdr[6];
    read_buffer(dev, hdr, 6);
    uint16_t next_pkt = (uint16_t)((uint16_t)hdr[1] << 8 | hdr[0]);
    uint16_t rx_len   = (uint16_t)((uint16_t)hdr[3] << 8 | hdr[2]);
    /* hdr[4..5] = status bits (ignored here) */

    /* p.46 Table 7-3: the count includes the CRC, which is the chip's and not
     * the caller's. It comes from the buffer, so it is not trusted to hold one. */
    if (rx_len < CRC_LEN) {
        free_packet(dev, next_pkt);
        *frame_len = 0;
        return ENC28J60_ERR_SIZE;
    }
    uint16_t len = (uint16_t)(rx_len - CRC_LEN);

    if (len > buf_size) {
        /* Still need to free the packet even if buffer too small */
        free_packet(dev, next_pkt);
        *frame_len = len;
        return ENC28J60_ERR_SIZE;
    }

    /* Read frame data. The next receive sets ERDPT, so the CRC needs no reading. */
    read_buffer(dev, frame, len);
    *frame_len = len;

    free_packet(dev, next_pkt);

    return ENC28J60_OK;
}

bool enc28j60_has_packet(enc28j60_t *dev)
{
    if (!dev || !dev->spi) return false;
    /* EPKTCNT is an ETH register -- no dummy byte (see enc28j60_receive) */
    return read_reg(dev, 1, EPKTCNT, false) > 0;
}

uint8_t enc28j60_get_revision(enc28j60_t *dev)
{
    if (!dev || !dev->spi) return 0;
    /* EREVID (bank3:0x12) is an ETH register per datasheet table 3-1,
     * not MAC/MII -- no dummy byte required. */
    return read_reg(dev, 3, EREVID, false);
}

enc28j60_err_t enc28j60_set_mac(enc28j60_t *dev, const uint8_t *mac_addr)
{
    if (!dev || !dev->spi) return ENC28J60_ERR_PARAM;
    if (!mac_addr)         return ENC28J60_ERR_PARAM;

    /* DS39662C p.45: clear RXEN before altering the MAC address. A frame
     * already arriving still has to finish. */
    bool rx_on = (spi_read_op(dev->spi, OP_RCR, ECON1, false) & ECON1_RXEN) != 0U;
    bit_field_clear(dev, ECON1, ECON1_RXEN);

    enc28j60_err_t err = wait_estat(dev, ESTAT_RXBUSY, 0U);
    if (err == ENC28J60_OK) {
        memcpy(dev->mac_addr, mac_addr, 6);
        write_mac_regs(dev);
    }

    if (rx_on) {
        bit_field_set(dev, ECON1, ECON1_RXEN);
    }
    return err;
}

void enc28j60_enable_reception(enc28j60_t *dev)
{
    if (!dev || !dev->spi) return;
    bit_field_set(dev, ECON1, ECON1_RXEN);
}
