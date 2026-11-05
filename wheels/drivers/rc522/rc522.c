// SPDX-License-Identifier: MIT
/*
 * rc522 — MFRC522 SPI RFID/NFC reader driver
 *
 * Layer:     L1
 * Status:    candidate
 * Deps:      spi_hal (injected)
 * Instances: multi
 *
 * MFRC522 SPI protocol:
 *   Address byte: bit7 = 1 for read, 0 for write
 *                 bits6:1 = register address
 *                 bit0 = 0 (always)
 *   Followed by data byte(s).
 */

#include "rc522.h"
#include "contract.h"
#include <stddef.h>

/* ---------- SPI helpers ---------- */

static void reg_write(const rc522_spi_t *spi, uint8_t reg, uint8_t val)
{
    /* Write: bit7=0, bits6:1 = addr, bit0=0 */
    uint8_t addr = (uint8_t)((reg << 1) & 0x7Eu);
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, addr);
    spi->xfer(spi->ctx, val);
    spi->cs_high(spi->ctx);
}

static uint8_t reg_read(const rc522_spi_t *spi, uint8_t reg)
{
    /* Read: bit7=1, bits6:1 = addr, bit0=0 */
    uint8_t addr = (uint8_t)(0x80u | ((reg << 1) & 0x7Eu));
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, addr);
    uint8_t val = spi->xfer(spi->ctx, 0x00);
    spi->cs_high(spi->ctx);
    return val;
}

static void reg_set_bits(const rc522_spi_t *spi, uint8_t reg, uint8_t mask)
{
    uint8_t val = reg_read(spi, reg);
    reg_write(spi, reg, (uint8_t)(val | mask));
}

static void reg_clear_bits(const rc522_spi_t *spi, uint8_t reg, uint8_t mask)
{
    uint8_t val = reg_read(spi, reg);
    reg_write(spi, reg, (uint8_t)(val & (uint8_t)~mask));
}

/* Read multiple bytes from FIFO */
static void fifo_read(const rc522_spi_t *spi, uint8_t *buf, uint8_t len)
{
    if (len == 0) return;
    /* Burst read from FIFO register */
    uint8_t addr = (uint8_t)(0x80u | ((RC522_REG_FIFODATA << 1) & 0x7Eu));
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, addr);
    for (uint8_t i = 0; i < len; i++) {
        /* Send the same address for each byte except last (send 0 for last) */
        buf[i] = spi->xfer(spi->ctx, (i < (uint8_t)(len - 1)) ? addr : 0x00);
    }
    spi->cs_high(spi->ctx);
}

/* Write data to FIFO */
static void fifo_write(const rc522_spi_t *spi, const uint8_t *buf, uint8_t len)
{
    uint8_t addr = (uint8_t)((RC522_REG_FIFODATA << 1) & 0x7Eu);
    spi->cs_low(spi->ctx);
    spi->xfer(spi->ctx, addr);
    for (uint8_t i = 0; i < len; i++) {
        spi->xfer(spi->ctx, buf[i]);
    }
    spi->cs_high(spi->ctx);
}

/* Transceive: send data to PICC and receive response via FIFO */
static rc522_err_t transceive(const rc522_t *dev,
                               const uint8_t *tx, uint8_t tx_len,
                               uint8_t *rx, uint8_t *rx_len,
                               uint8_t tx_last_bits)
{
    const rc522_spi_t *spi = dev->spi;

    reg_write(spi, RC522_REG_COMMAND, RC522_CMD_IDLE);
    reg_write(spi, RC522_REG_COMIRQ, 0x7F);        /* clear all IRQ bits */
    reg_write(spi, RC522_REG_FIFOLEVEL, 0x80);      /* flush FIFO */

    /* Write data to FIFO */
    fifo_write(spi, tx, tx_len);

    /* Set number of valid bits in last byte (BitFramingReg) */
    reg_write(spi, RC522_REG_BITFRAMING, (uint8_t)(tx_last_bits & 0x07u));

    /* Execute Transceive command */
    reg_write(spi, RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);

    /* Start transmission: set StartSend bit (bit 7 of BitFramingReg) */
    reg_set_bits(spi, RC522_REG_BITFRAMING, 0x80);

    /* Wait for completion (poll ComIrqReg) */
    /* Bits: RxIRq (bit 5) = data received, IdleIRq (bit 4) = command done,
     * TimerIRq (bit 0) = timeout */
    uint16_t wait_cycles = 2000;
    uint8_t irq;
    do {
        irq = reg_read(spi, RC522_REG_COMIRQ);
        if (irq & 0x30u) break;    /* RxIRq or IdleIRq */
        if (irq & 0x01u) break;    /* TimerIRq */
        wait_cycles--;
    } while (wait_cycles > 0);

    /* Clear StartSend bit */
    reg_clear_bits(spi, RC522_REG_BITFRAMING, 0x80);

    if (wait_cycles == 0 || (irq & 0x01u)) {
        return RC522_ERR_TIMEOUT;
    }

    /* Check for errors */
    uint8_t err = reg_read(spi, RC522_REG_ERROR);
    if (err & 0x1Bu) {  /* BufferOvfl(4), CollErr(3), ParityErr(1), ProtocolErr(0) */
        if (err & 0x08u) return RC522_ERR_COLL;
        return RC522_ERR_SPI;
    }

    /* Read response from FIFO */
    if (rx && rx_len) {
        uint8_t fifo_len = (uint8_t)(reg_read(spi, RC522_REG_FIFOLEVEL) & 0x7Fu);
        if (fifo_len > *rx_len) fifo_len = *rx_len;
        *rx_len = fifo_len;
        if (fifo_len > 0) {
            fifo_read(spi, rx, fifo_len);
        }
    }

    return RC522_OK;
}

/* ---------- public API ---------- */

/* Everything a soft reset clears. */
static void configure(const rc522_spi_t *spi)
{
    /* Configure timer: TModeReg and TPrescalerReg
     * TAuto=1, f_timer = 13.56MHz / (2*TPreScaler+1)
     * TPrescaler = 0xD3E => ~2kHz timer, reload=30 => ~15ms timeout */
    reg_write(spi, RC522_REG_TMODE, 0x8D);         /* TAuto=1, prescaler high nibble */
    reg_write(spi, RC522_REG_TPRESCALER, 0x3E);    /* prescaler low byte */
    reg_write(spi, RC522_REG_TRELOAD_HI, 0x00);    /* reload value high */
    reg_write(spi, RC522_REG_TRELOAD_LO, 0x1E);    /* reload value low = 30 */

    /* Force 100% ASK modulation */
    reg_write(spi, RC522_REG_TXASK, 0x40);

    /* Set CRC preset to 0x6363 (ISO 14443-3) */
    reg_write(spi, RC522_REG_MODE, 0x3D);

    /* TxControlReg resets to 80h, so there is no RF field until these are set */
    reg_set_bits(spi, RC522_REG_TXCONTROL, 0x03);
}

rc522_err_t rc522_init(rc522_t *dev,
                           const rc522_spi_t *spi)
{
    if (!dev || !spi) return RC522_ERR_PARAM;
    if (!spi->cs_low || !spi->cs_high || !spi->xfer) return RC522_ERR_PARAM;
    dev->spi = spi;
    dev->version = 0;

    /* Soft reset */
    reg_write(spi, RC522_REG_COMMAND, RC522_CMD_SOFTRESET);
    if (spi->delay_ms) {
        spi->delay_ms(spi->ctx, 50);
    }

    /* Read and verify version register */
    uint8_t ver = reg_read(spi, RC522_REG_VERSION);
    if (ver != RC522_VERSION_1_0 && ver != RC522_VERSION_2_0) {
        return RC522_ERR_ID;
    }
    dev->version = ver;

    configure(spi);

    INVARIANT(dev->spi != NULL);
    INVARIANT(dev->version == RC522_VERSION_1_0 || dev->version == RC522_VERSION_2_0);
    return RC522_OK;
}

rc522_err_t rc522_self_test(rc522_t *dev)
{
    if (!dev || !dev->spi) return RC522_ERR_PARAM;
    const rc522_spi_t *spi = dev->spi;

    /* Perform self-test per datasheet section 16.1.1:
     * 1. Soft reset
     * 2. Flush FIFO, write 25 zero bytes
     * 3. Enable self-test via AutoTestReg
     * 4. Write 0x00 to FIFO
     * 5. Start CalcCRC command
     * 6. Wait for 64 bytes in FIFO
     */
    reg_write(spi, RC522_REG_COMMAND, RC522_CMD_SOFTRESET);
    if (spi->delay_ms) {
        spi->delay_ms(spi->ctx, 50);
    }

    /* Flush FIFO */
    reg_write(spi, RC522_REG_FIFOLEVEL, 0x80);

    /* Write 25 bytes of 0x00 to internal buffer */
    {
        uint8_t zeros[25];
        for (uint8_t i = 0; i < 25; i++) zeros[i] = 0x00;
        fifo_write(spi, zeros, 25);
    }

    /* Transfer to internal buffer */
    reg_write(spi, RC522_REG_COMMAND, RC522_CMD_MEM);

    /* Enable self test */
    reg_write(spi, RC522_REG_AUTOTEST, 0x09);

    /* Write 0x00 to FIFO again */
    reg_write(spi, RC522_REG_FIFODATA, 0x00);

    /* Start CRC calculation */
    reg_write(spi, RC522_REG_COMMAND, RC522_CMD_CALCCRC);

    /* Wait for 64 bytes in FIFO */
    uint16_t wait = 500;
    while (wait > 0) {
        uint8_t n = reg_read(spi, RC522_REG_FIFOLEVEL);
        if (n >= 64) break;
        wait--;
    }

    reg_write(spi, RC522_REG_COMMAND, RC522_CMD_IDLE);
    reg_write(spi, RC522_REG_AUTOTEST, 0x00);

    if (wait == 0) return RC522_ERR_TIMEOUT;

    /* Read and check: at least one non-zero byte expected */
    uint8_t result[64];
    fifo_read(spi, result, 64);

    bool all_zero = true;
    for (uint8_t i = 0; i < 64; i++) {
        if (result[i] != 0x00) { all_zero = false; break; }
    }
    if (all_zero) return RC522_ERR_SPI;

    return RC522_OK;
}

rc522_err_t rc522_detect_card(rc522_t *dev)
{
    if (!dev || !dev->spi) return RC522_ERR_PARAM;

    /* Send REQA (0x26) with 7 valid bits */
    uint8_t reqa = RC522_PICC_REQA;
    uint8_t atqa[2];
    uint8_t atqa_len = 2;

    rc522_err_t err = transceive(dev, &reqa, 1, atqa, &atqa_len, 7);
    if (err != RC522_OK) return err;
    if (atqa_len != 2) return RC522_ERR_TIMEOUT;

    return RC522_OK;
}

rc522_err_t rc522_read_card_uid(rc522_t *dev, rc522_uid_t *uid)
{
    if (!dev || !dev->spi || !uid) return RC522_ERR_PARAM;

    /* Anticollision cascade level 1 */
    uint8_t cmd[2] = { RC522_PICC_ANTICOLL_CL1, 0x20 };  /* 0x20 = 2 valid bytes */
    uint8_t resp[5];  /* 4 UID bytes + BCC */
    uint8_t resp_len = 5;

    rc522_err_t err = transceive(dev, cmd, 2, resp, &resp_len, 0);
    if (err != RC522_OK) return err;
    if (resp_len < 5) return RC522_ERR_TIMEOUT;

    /* Verify BCC (XOR of 4 UID bytes) */
    uint8_t bcc = (uint8_t)(resp[0] ^ resp[1] ^ resp[2] ^ resp[3]);
    if (bcc != resp[4]) return RC522_ERR_COLL;

    uid->len = 4;
    uid->bytes[0] = resp[0];
    uid->bytes[1] = resp[1];
    uid->bytes[2] = resp[2];
    uid->bytes[3] = resp[3];

    return RC522_OK;
}

rc522_err_t rc522_halt(rc522_t *dev)
{
    if (!dev || !dev->spi) return RC522_ERR_PARAM;

    uint8_t cmd[2] = { RC522_PICC_HALT_CMD, 0x00 };

    /* HALT doesn't expect a response (card goes silent) */
    rc522_err_t err = transceive(dev, cmd, 2, NULL, NULL, 0);
    /* Timeout is expected -- card goes to HALT state and stops responding */
    if (err == RC522_ERR_TIMEOUT) return RC522_OK;
    return err;
}

rc522_err_t rc522_antenna_on(rc522_t *dev)
{
    if (!dev || !dev->spi) return RC522_ERR_PARAM;

    /* Set Tx1RFEn and Tx2RFEn bits in TxControlReg */
    reg_set_bits(dev->spi, RC522_REG_TXCONTROL, 0x03);
    return RC522_OK;
}

uint8_t rc522_get_version(const rc522_t *dev)
{
    if (!dev) return 0;
    return dev->version;
}
